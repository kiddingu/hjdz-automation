#include "taskmodel.h"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QFileInfo>

// ============== 步骤类型转换 ==============

QString stepTypeToString(StepType type) {
    switch (type) {
        case StepType::WaitClick:     return "wait_click";
        case StepType::WaitAppear:    return "wait_appear";
        case StepType::WaitDisappear: return "wait_disappear";
        case StepType::Click:         return "click";
        case StepType::ClickPos:      return "click_pos";
        case StepType::Sleep:         return "sleep";
        case StepType::IfExist:       return "if_exist";
        case StepType::IfExistClick:  return "if_exist_click";
        case StepType::Loop:          return "loop";
        case StepType::LoopUntil:     return "loop_until";
        case StepType::Goto:          return "goto";
        case StepType::EndSuccess:    return "end_success";
        case StepType::EndFail:       return "end_fail";
    }
    return "wait_click";
}

StepType stringToStepType(const QString& str) {
    if (str == "wait_click")     return StepType::WaitClick;
    if (str == "wait_appear")    return StepType::WaitAppear;
    if (str == "wait_disappear") return StepType::WaitDisappear;
    if (str == "click")          return StepType::Click;
    if (str == "click_pos")      return StepType::ClickPos;
    if (str == "sleep")          return StepType::Sleep;
    if (str == "if_exist")       return StepType::IfExist;
    if (str == "if_exist_click") return StepType::IfExistClick;
    if (str == "loop")           return StepType::Loop;
    if (str == "loop_until")     return StepType::LoopUntil;
    if (str == "goto")           return StepType::Goto;
    if (str == "end_success")    return StepType::EndSuccess;
    if (str == "end_fail")       return StepType::EndFail;
    return StepType::WaitClick;
}

// ============== TaskStep ==============

TaskStep TaskStep::fromJson(const QJsonObject& json) {
    TaskStep step;
    step.id = json["id"].toString();
    step.type = stringToStepType(json["type"].toString());
    step.description = json["description"].toString();

    // 图片可以是字符串或数组
    if (json["image"].isString()) {
        step.images << json["image"].toString();
    } else if (json["image"].isArray()) {
        for (const auto& v : json["image"].toArray()) {
            step.images << v.toString();
        }
    }

    step.matchMode = json["match_mode"].toString("any");
    step.threshold = json["threshold"].toDouble(0.85);
    step.timeout = json["timeout"].toInt(8000);
    step.sleepMs = json["sleep_ms"].toInt(json["then_sleep"].toInt(0));
    step.onSuccess = json["on_success"].toString();
    step.onFail = json["on_fail"].toString();
    step.maxIterations = json["max_iterations"].toInt(1);
    step.loopUntilImage = json["until_image"].toString();
    step.failReason = json["reason"].toString();

    // 点击偏移
    if (json.contains("offset")) {
        auto off = json["offset"].toObject();
        step.clickOffset = QPoint(off["x"].toInt(0), off["y"].toInt(0));
    }
    if (json.contains("x") && json.contains("y")) {
        step.clickOffset = QPoint(json["x"].toInt(), json["y"].toInt());
    }

    // 子步骤
    if (json.contains("steps")) {
        for (const auto& v : json["steps"].toArray()) {
            step.subSteps << TaskStep::fromJson(v.toObject());
        }
    }
    // then/else 分支也作为子步骤处理
    if (json.contains("then")) {
        auto thenVal = json["then"];
        if (thenVal.isObject()) {
            step.subSteps << TaskStep::fromJson(thenVal.toObject());
        } else if (thenVal.isString()) {
            TaskStep gotoStep;
            gotoStep.type = StepType::Goto;
            gotoStep.onSuccess = thenVal.toString();
            step.subSteps << gotoStep;
        }
    }

    return step;
}

QJsonObject TaskStep::toJson() const {
    QJsonObject json;
    if (!id.isEmpty()) json["id"] = id;
    json["type"] = stepTypeToString(type);
    if (!description.isEmpty()) json["description"] = description;

    // 图片
    if (images.size() == 1) {
        json["image"] = images.first();
    } else if (images.size() > 1) {
        QJsonArray arr;
        for (const auto& img : images) arr << img;
        json["image"] = arr;
    }

    if (matchMode != "any") json["match_mode"] = matchMode;
    if (threshold != 0.85) json["threshold"] = threshold;
    if (timeout != 8000) json["timeout"] = timeout;
    if (sleepMs > 0) json["sleep_ms"] = sleepMs;
    if (!onSuccess.isEmpty()) json["on_success"] = onSuccess;
    if (!onFail.isEmpty()) json["on_fail"] = onFail;
    if (maxIterations != 1) json["max_iterations"] = maxIterations;
    if (!loopUntilImage.isEmpty()) json["until_image"] = loopUntilImage;
    if (!failReason.isEmpty()) json["reason"] = failReason;

    if (!clickOffset.isNull()) {
        if (type == StepType::ClickPos) {
            json["x"] = clickOffset.x();
            json["y"] = clickOffset.y();
        } else {
            QJsonObject off;
            off["x"] = clickOffset.x();
            off["y"] = clickOffset.y();
            json["offset"] = off;
        }
    }

    if (!subSteps.isEmpty()) {
        QJsonArray arr;
        for (const auto& sub : subSteps) arr << sub.toJson();
        json["steps"] = arr;
    }

    return json;
}

QString TaskStep::displayName() const {
    QString typeStr;
    switch (type) {
        case StepType::WaitClick:     typeStr = QStringLiteral("等待点击"); break;
        case StepType::WaitAppear:    typeStr = QStringLiteral("等待出现"); break;
        case StepType::WaitDisappear: typeStr = QStringLiteral("等待消失"); break;
        case StepType::Click:         typeStr = QStringLiteral("点击"); break;
        case StepType::ClickPos:      typeStr = QStringLiteral("点击坐标"); break;
        case StepType::Sleep:         typeStr = QStringLiteral("延迟"); break;
        case StepType::IfExist:       typeStr = QStringLiteral("条件判断"); break;
        case StepType::IfExistClick:  typeStr = QStringLiteral("存在则点"); break;
        case StepType::Loop:          typeStr = QStringLiteral("循环"); break;
        case StepType::LoopUntil:     typeStr = QStringLiteral("循环直到"); break;
        case StepType::Goto:          typeStr = QStringLiteral("跳转"); break;
        case StepType::EndSuccess:    typeStr = QStringLiteral("成功结束"); break;
        case StepType::EndFail:       typeStr = QStringLiteral("失败结束"); break;
    }

    if (!description.isEmpty()) {
        return QString("[%1] %2").arg(typeStr, description);
    }
    if (!images.isEmpty()) {
        QString imgName = QFileInfo(images.first()).baseName();
        return QString("[%1] %2").arg(typeStr, imgName);
    }
    if (type == StepType::Sleep) {
        return QString("[%1] %2ms").arg(typeStr).arg(sleepMs);
    }
    if (type == StepType::ClickPos) {
        return QString("[%1] (%2, %3)").arg(typeStr).arg(clickOffset.x()).arg(clickOffset.y());
    }
    if (type == StepType::Goto && !onSuccess.isEmpty()) {
        return QString("[%1] -> %2").arg(typeStr, onSuccess);
    }
    return QString("[%1]").arg(typeStr);
}

// ============== TaskDefinition ==============

TaskDefinition TaskDefinition::fromJson(const QJsonObject& json) {
    TaskDefinition task;
    task.name = json["name"].toString();
    task.description = json["description"].toString();
    task.imageFolder = json["image_folder"].toString();
    task.version = json["version"].toInt(1);

    for (const auto& v : json["steps"].toArray()) {
        task.steps << TaskStep::fromJson(v.toObject());
    }

    return task;
}

QJsonObject TaskDefinition::toJson() const {
    QJsonObject json;
    json["name"] = name;
    if (!description.isEmpty()) json["description"] = description;
    if (!imageFolder.isEmpty()) json["image_folder"] = imageFolder;
    json["version"] = version;

    QJsonArray stepsArr;
    for (const auto& step : steps) {
        stepsArr << step.toJson();
    }
    json["steps"] = stepsArr;

    return json;
}

bool TaskDefinition::saveToFile(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[TaskDefinition] Cannot write to file:" << path;
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

TaskDefinition TaskDefinition::loadFromFile(const QString& path) {
    TaskDefinition task;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[TaskDefinition] Cannot read file:" << path;
        return task;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[TaskDefinition] JSON parse error:" << error.errorString();
        return task;
    }

    return TaskDefinition::fromJson(doc.object());
}

bool TaskDefinition::isValid(QString* errorMsg) const {
    if (name.isEmpty()) {
        if (errorMsg) *errorMsg = QStringLiteral("任务名称不能为空");
        return false;
    }
    if (steps.isEmpty()) {
        if (errorMsg) *errorMsg = QStringLiteral("任务步骤不能为空");
        return false;
    }
    return true;
}

static void collectImages(const QList<TaskStep>& steps, QStringList& images) {
    for (const auto& step : steps) {
        for (const auto& img : step.images) {
            if (!img.isEmpty() && !images.contains(img)) {
                images << img;
            }
        }
        if (!step.loopUntilImage.isEmpty() && !images.contains(step.loopUntilImage)) {
            images << step.loopUntilImage;
        }
        collectImages(step.subSteps, images);
    }
}

QStringList TaskDefinition::getReferencedImages() const {
    QStringList images;
    collectImages(steps, images);
    return images;
}

// ============== TaskPackage ==============

bool TaskPackage::exportTask(const TaskDefinition& task,
                             const QString& imageFolder,
                             const QString& outputPath) {
    // 简化实现：将JSON和图片打包到zip格式
    // 由于Qt没有内置zip支持，这里使用简单的目录结构
    // 实际使用时可以集成 QuaZip 库

    QFileInfo outInfo(outputPath);
    QString tempDir = outInfo.absolutePath() + "/" + outInfo.baseName() + "_export";

    QDir dir;
    dir.mkpath(tempDir);
    dir.mkpath(tempDir + "/images");

    // 保存任务JSON
    task.saveToFile(tempDir + "/task.json");

    // 复制图片
    QDir imgDir(imageFolder);
    QStringList images = task.getReferencedImages();
    for (const auto& img : images) {
        QString srcPath = imgDir.absoluteFilePath(img);
        QString dstPath = tempDir + "/images/" + QFileInfo(img).fileName();
        if (QFile::exists(srcPath)) {
            QFile::copy(srcPath, dstPath);
        }
    }

    qDebug() << "[TaskPackage] Exported to:" << tempDir;
    qDebug() << "[TaskPackage] Note: Use external tool to create .hjdz (zip) file";

    return true;
}

bool TaskPackage::importTask(const QString& packagePath,
                             const QString& targetTaskDir,
                             const QString& targetImageDir,
                             TaskDefinition* outTask) {
    // 简化实现：假设packagePath是一个目录
    QDir pkgDir(packagePath);
    if (!pkgDir.exists()) {
        qWarning() << "[TaskPackage] Package not found:" << packagePath;
        return false;
    }

    // 读取任务JSON
    QString jsonPath = pkgDir.absoluteFilePath("task.json");
    TaskDefinition task = TaskDefinition::loadFromFile(jsonPath);
    if (task.name.isEmpty()) {
        qWarning() << "[TaskPackage] Invalid task.json";
        return false;
    }

    // 复制图片
    QDir imgSrcDir(pkgDir.absoluteFilePath("images"));
    QDir imgDstDir(targetImageDir);
    imgDstDir.mkpath(".");

    for (const auto& entry : imgSrcDir.entryInfoList(QDir::Files)) {
        QString dstPath = imgDstDir.absoluteFilePath(entry.fileName());
        if (!QFile::exists(dstPath)) {
            QFile::copy(entry.absoluteFilePath(), dstPath);
        }
    }

    // 保存任务到目标目录
    QDir taskDir(targetTaskDir);
    taskDir.mkpath(".");
    task.saveToFile(taskDir.absoluteFilePath(task.name + ".json"));

    if (outTask) *outTask = task;
    return true;
}
