#include "scriptrunner.h"
#include "automationworker.h"
#include <QElapsedTimer>
#include <QThread>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>

ScriptRunner::ScriptRunner(AutomationWorker* worker, QObject* parent)
    : QObject(parent), worker_(worker)
{
}

ScriptRunner::~ScriptRunner() {
    stop();
}

void ScriptRunner::stop() {
    stopped_.store(true);
}

bool ScriptRunner::shouldStop() const {
    return stopped_.load() || (worker_ && worker_->shouldStop("ScriptRunner"));
}

void ScriptRunner::sleepMs(int ms) {
    if (worker_) {
        worker_->sleepMs(ms);
    } else {
        QThread::msleep(static_cast<unsigned long>(ms));
    }
}

bool ScriptRunner::execute(const TaskDefinition& task) {
    if (task.steps.isEmpty()) {
        emit log(QStringLiteral("[脚本] 任务步骤为空"));
        emit taskFinished(false, QStringLiteral("任务步骤为空"));
        return false;
    }

    currentTask_ = task;
    stopped_.store(false);
    running_.store(true);
    stepIndex_.clear();

    // 构建步骤ID索引
    for (int i = 0; i < task.steps.size(); ++i) {
        if (!task.steps[i].id.isEmpty()) {
            stepIndex_[task.steps[i].id] = i;
        }
    }

    emit log(QStringLiteral("[脚本] 开始执行任务: %1").arg(task.name));

    int currentIndex = 0;
    bool success = true;
    QString lastResult;

    while (currentIndex >= 0 && currentIndex < task.steps.size()) {
        if (shouldStop()) {
            emit log(QStringLiteral("[脚本] 任务被中断"));
            emit taskFinished(false, QStringLiteral("任务被用户中断"));
            running_.store(false);
            return false;
        }

        const TaskStep& step = task.steps[currentIndex];
        emit stepStarted(step.id, step.displayName());
        emit log(QStringLiteral("[脚本] 执行步骤 %1: %2").arg(currentIndex + 1).arg(step.displayName()));

        QString nextStepId = executeStep(step);
        bool stepSuccess = !nextStepId.startsWith("__FAIL__");

        emit stepCompleted(step.id, stepSuccess);

        // 处理特殊结果
        if (nextStepId == "__END_SUCCESS__") {
            emit log(QStringLiteral("[脚本] 任务成功完成"));
            emit taskFinished(true, QStringLiteral("任务成功完成"));
            running_.store(false);
            return true;
        }
        if (nextStepId.startsWith("__END_FAIL__")) {
            QString reason = nextStepId.mid(12);
            emit log(QStringLiteral("[脚本] 任务失败: %1").arg(reason));
            emit taskFinished(false, reason);
            running_.store(false);
            return false;
        }

        // 确定下一步
        if (nextStepId.isEmpty()) {
            // 顺序执行下一步
            currentIndex++;
        } else if (nextStepId == "__NEXT__") {
            currentIndex++;
        } else {
            // 跳转到指定步骤
            int idx = findStepIndexById(nextStepId);
            if (idx >= 0) {
                currentIndex = idx;
            } else {
                emit log(QStringLiteral("[脚本] 警告：找不到步骤 %1，继续执行下一步").arg(nextStepId));
                currentIndex++;
            }
        }
    }

    emit log(QStringLiteral("[脚本] 任务执行完毕"));
    emit taskFinished(true, QStringLiteral("所有步骤执行完成"));
    running_.store(false);
    return true;
}

QString ScriptRunner::executeStep(const TaskStep& step) {
    bool success = false;

    switch (step.type) {
        case StepType::WaitClick:
            success = executeWaitClick(step);
            break;
        case StepType::WaitAppear:
            success = executeWaitAppear(step);
            break;
        case StepType::WaitDisappear:
            success = executeWaitDisappear(step);
            break;
        case StepType::Click:
            success = executeClick(step);
            break;
        case StepType::ClickPos:
            success = executeClickPos(step);
            break;
        case StepType::Sleep:
            success = executeSleep(step);
            break;
        case StepType::IfExist:
            success = executeIfExist(step);
            // IfExist 自己处理跳转
            return success ? step.onSuccess : step.onFail;
        case StepType::IfExistClick:
            success = executeIfExistClick(step);
            break;
        case StepType::Loop:
            success = executeLoop(step);
            break;
        case StepType::LoopUntil:
            success = executeLoopUntil(step);
            break;
        case StepType::Goto:
            return step.onSuccess;
        case StepType::EndSuccess:
            return "__END_SUCCESS__";
        case StepType::EndFail:
            return "__END_FAIL__" + step.failReason;
    }

    if (success) {
        return step.onSuccess.isEmpty() ? "__NEXT__" : step.onSuccess;
    } else {
        if (!step.onFail.isEmpty()) {
            // 用户指定了失败时的跳转
            return step.onFail;
        }
        // 默认失败行为：记录错误但继续执行下一步
        // 用户如果想失败时停止，可以设置 onFail 为 "end_fail"
        emit log(QStringLiteral("[脚本] 步骤 [%1] 失败，继续执行下一步").arg(step.displayName()));
        return "__NEXT__";
    }
}

bool ScriptRunner::executeWaitClick(const TaskStep& step) {
    QPoint pos;

    // 最多重试3次
    int maxRetry = 3;
    for (int retry = 0; retry < maxRetry; ++retry) {
        if (shouldStop()) return false;

        if (retry > 0) {
            emit log(QStringLiteral("[脚本] 重试第 %1 次...").arg(retry));
            sleepMs(500);
        }

        if (waitForImage(step.images, step.threshold, step.timeout, step.matchMode, &pos)) {
            pos += step.clickOffset;
            lastMatchedPos_ = pos;

            if (clickAtPoint(pos)) {
                if (step.sleepMs > 0) {
                    sleepMs(step.sleepMs);
                }
                return true;
            } else {
                emit log(QStringLiteral("[脚本] 点击失败，位置: (%1, %2)").arg(pos.x()).arg(pos.y()));
            }
        } else {
            emit log(QStringLiteral("[脚本] 等待图片超时: %1").arg(step.images.join(", ")));
        }
    }

    emit log(QStringLiteral("[脚本] 步骤失败(重试%1次): %2").arg(maxRetry).arg(step.displayName()));
    return false;
}

bool ScriptRunner::executeWaitAppear(const TaskStep& step) {
    QPoint pos;

    // 最多重试2次
    int maxRetry = 2;
    for (int retry = 0; retry < maxRetry; ++retry) {
        if (shouldStop()) return false;

        if (retry > 0) {
            emit log(QStringLiteral("[脚本] 重试第 %1 次...").arg(retry));
            sleepMs(500);
        }

        if (waitForImage(step.images, step.threshold, step.timeout, step.matchMode, &pos)) {
            lastMatchedPos_ = pos;
            return true;
        } else {
            emit log(QStringLiteral("[脚本] 等待图片超时: %1").arg(step.images.join(", ")));
        }
    }

    emit log(QStringLiteral("[脚本] 步骤失败: %1").arg(step.displayName()));
    return false;
}

bool ScriptRunner::executeWaitDisappear(const TaskStep& step) {
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < step.timeout) {
        if (shouldStop()) return false;

        bool exists = false;
        for (const auto& img : step.images) {
            if (checkImageExists(img, step.threshold)) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            return true;
        }

        sleepMs(200);
    }

    return false;
}

bool ScriptRunner::executeClick(const TaskStep& step) {
    Q_UNUSED(step)
    // 点击上次匹配到的位置
    if (lastMatchedPos_.isNull()) {
        emit log(QStringLiteral("[脚本] 警告：没有上次匹配位置，无法点击"));
        return false;
    }
    return clickAtPoint(lastMatchedPos_);
}

bool ScriptRunner::executeClickPos(const TaskStep& step) {
    return clickAtPoint(step.clickOffset);
}

bool ScriptRunner::executeSleep(const TaskStep& step) {
    sleepMs(step.sleepMs);
    return true;
}

bool ScriptRunner::executeIfExist(const TaskStep& step) {
    QPoint pos;
    for (const auto& img : step.images) {
        if (checkImageExists(img, step.threshold, &pos)) {
            lastMatchedPos_ = pos;
            return true;
        }
    }
    return false;
}

bool ScriptRunner::executeIfExistClick(const TaskStep& step) {
    QPoint pos;
    for (const auto& img : step.images) {
        if (checkImageExists(img, step.threshold, &pos)) {
            pos += step.clickOffset;
            lastMatchedPos_ = pos;
            clickAtPoint(pos);

            if (step.sleepMs > 0) {
                sleepMs(step.sleepMs);
            }
            return true;
        }
    }
    return false;
}

bool ScriptRunner::executeLoop(const TaskStep& step) {
    for (int i = 0; i < step.maxIterations; ++i) {
        if (shouldStop()) return false;

        emit log(QStringLiteral("[脚本] 循环 %1/%2").arg(i + 1).arg(step.maxIterations));

        // 执行子步骤
        for (const auto& subStep : step.subSteps) {
            if (shouldStop()) return false;
            executeStep(subStep);
        }

        // 检查循环终止条件
        if (!step.loopUntilImage.isEmpty()) {
            if (checkImageExists(step.loopUntilImage, step.threshold)) {
                emit log(QStringLiteral("[脚本] 检测到终止条件，退出循环"));
                break;
            }
        }
    }
    return true;
}

bool ScriptRunner::executeLoopUntil(const TaskStep& step) {
    for (int i = 0; i < step.maxIterations; ++i) {
        if (shouldStop()) return false;

        // 检查终止条件
        if (!step.loopUntilImage.isEmpty()) {
            if (checkImageExists(step.loopUntilImage, step.threshold)) {
                emit log(QStringLiteral("[脚本] 条件满足，退出循环"));
                return true;
            }
        }

        emit log(QStringLiteral("[脚本] 循环直到 %1/%2").arg(i + 1).arg(step.maxIterations));

        // 执行子步骤
        for (const auto& subStep : step.subSteps) {
            if (shouldStop()) return false;
            executeStep(subStep);
        }
    }

    emit log(QStringLiteral("[脚本] 达到最大循环次数"));
    return false;
}

bool ScriptRunner::waitForImage(const QStringList& images, double threshold, int timeout,
                                 const QString& matchMode, QPoint* outPos) {
    if (images.isEmpty()) return false;

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout) {
        if (shouldStop()) return false;

        if (matchMode == "all") {
            // 所有图片都要匹配
            bool allMatched = true;
            QPoint firstPos;
            for (const auto& img : images) {
                QPoint pos;
                if (!checkImageExists(img, threshold, &pos)) {
                    allMatched = false;
                    break;
                }
                if (firstPos.isNull()) firstPos = pos;
            }
            if (allMatched) {
                if (outPos) *outPos = firstPos;
                return true;
            }
        } else {
            // 任意一个图片匹配即可
            for (const auto& img : images) {
                QPoint pos;
                if (checkImageExists(img, threshold, &pos)) {
                    if (outPos) *outPos = pos;
                    return true;
                }
            }
        }

        sleepMs(200);
    }

    return false;
}

bool ScriptRunner::checkImageExists(const QString& image, double threshold, QPoint* outPos) {
    if (!worker_) return false;

    // 解析图片路径
    QString imagePath = image;

    // 获取应用程序目录作为基础路径
    QString appDir = QCoreApplication::applicationDirPath();

    if (!image.contains('/') && !image.contains('\\')) {
        // 纯文件名，添加任务图片文件夹前缀
        if (!currentTask_.imageFolder.isEmpty()) {
            imagePath = appDir + "/" + currentTask_.imageFolder + "/" + image;
        } else {
            imagePath = appDir + "/" + image;
        }
    } else if (!QFileInfo(image).isAbsolute()) {
        // 相对路径，转换为绝对路径
        imagePath = appDir + "/" + image;
    }

    // 检查文件是否存在
    if (!QFile::exists(imagePath)) {
        emit log(QStringLiteral("[脚本] 图片文件不存在: %1").arg(imagePath));
        return false;
    }

    // 直接使用 worker 的方法进行图像匹配，避免使用全局 toolbox
    // 这样可以避免多窗口同时执行时的竞态条件
    QImage screen = worker_->capture();
    if (screen.isNull()) {
        emit log(QStringLiteral("[脚本] 无法捕获屏幕"));
        return false;
    }

    double score = 0.0;
    QPoint pt = worker_->findTemplatePlaceholder(screen, imagePath, &score, threshold);
    if (pt.x() >= 0) {
        if (outPos) *outPos = pt;
        return true;
    }

    return false;
}

bool ScriptRunner::clickAtPoint(const QPoint& pos) {
    if (!worker_) return false;
    return worker_->clickAt(pos);
}

const TaskStep* ScriptRunner::findStepById(const QString& id) const {
    int idx = findStepIndexById(id);
    if (idx >= 0 && idx < currentTask_.steps.size()) {
        return &currentTask_.steps[idx];
    }
    return nullptr;
}

int ScriptRunner::findStepIndexById(const QString& id) const {
    return stepIndex_.value(id, -1);
}
