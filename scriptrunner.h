#ifndef SCRIPTRUNNER_H
#define SCRIPTRUNNER_H

#include <QObject>
#include <QMap>
#include <atomic>
#include "taskmodel.h"

class AutomationWorker;

// 脚本任务执行引擎
// 解释执行 TaskDefinition 中定义的步骤
class ScriptRunner : public QObject {
    Q_OBJECT
public:
    explicit ScriptRunner(AutomationWorker* worker, QObject* parent = nullptr);
    ~ScriptRunner();

    // 执行任务
    bool execute(const TaskDefinition& task);

    // 停止执行
    void stop();

    // 是否正在运行
    bool isRunning() const { return running_.load(); }

signals:
    // 步骤开始执行
    void stepStarted(const QString& stepId, const QString& description);
    // 步骤执行完成
    void stepCompleted(const QString& stepId, bool success);
    // 任务完成
    void taskFinished(bool success, const QString& message);
    // 日志输出
    void log(const QString& msg);

private:
    // 执行单个步骤，返回下一步骤ID (空表示结束)
    QString executeStep(const TaskStep& step);

    // 各类型步骤的执行方法
    bool executeWaitClick(const TaskStep& step);
    bool executeWaitAppear(const TaskStep& step);
    bool executeWaitDisappear(const TaskStep& step);
    bool executeClick(const TaskStep& step);
    bool executeClickPos(const TaskStep& step);
    bool executeSleep(const TaskStep& step);
    bool executeIfExist(const TaskStep& step);
    bool executeIfExistClick(const TaskStep& step);
    bool executeLoop(const TaskStep& step);
    bool executeLoopUntil(const TaskStep& step);

    // 辅助方法
    bool waitForImage(const QStringList& images, double threshold, int timeout,
                      const QString& matchMode, QPoint* outPos = nullptr);
    bool checkImageExists(const QString& image, double threshold, QPoint* outPos = nullptr);
    bool clickAtPoint(const QPoint& pos);
    void sleepMs(int ms);
    bool shouldStop() const;

    // 根据步骤ID找到步骤
    const TaskStep* findStepById(const QString& id) const;
    int findStepIndexById(const QString& id) const;

    AutomationWorker* worker_;
    TaskDefinition currentTask_;
    QMap<QString, int> stepIndex_;      // id -> index 映射
    std::atomic<bool> stopped_{false};
    std::atomic<bool> running_{false};
    QPoint lastMatchedPos_;             // 上次匹配到的位置
};

#endif // SCRIPTRUNNER_H
