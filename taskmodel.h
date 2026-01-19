#ifndef TASKMODEL_H
#define TASKMODEL_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPoint>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

// 步骤类型枚举
enum class StepType {
    WaitClick,      // 等待图片出现并点击
    WaitAppear,     // 等待图片出现(不点击)
    WaitDisappear,  // 等待图片消失
    Click,          // 直接点击(上一步找到的位置)
    ClickPos,       // 点击指定坐标
    Sleep,          // 延迟等待
    IfExist,        // 条件判断(图片存在)
    IfExistClick,   // 存在则点击
    Loop,           // 循环执行
    LoopUntil,      // 循环直到条件满足
    Goto,           // 跳转到指定步骤
    EndSuccess,     // 任务成功结束
    EndFail         // 任务失败结束
};

// 步骤类型与字符串转换
QString stepTypeToString(StepType type);
StepType stringToStepType(const QString& str);

// 单个步骤定义
struct TaskStep {
    QString id;                 // 步骤ID (用于跳转)
    StepType type = StepType::WaitClick;
    QStringList images;         // 支持多图匹配
    QString matchMode = "any";  // "any" | "all"
    double threshold = 0.85;    // 匹配阈值
    int timeout = 8000;         // 超时时间(ms)
    int sleepMs = 0;            // 延迟时间(ms)
    QPoint clickOffset;         // 点击偏移
    QString onSuccess;          // 成功后跳转的步骤ID
    QString onFail;             // 失败后跳转的步骤ID
    QList<TaskStep> subSteps;   // 子步骤(用于循环/条件)
    int maxIterations = 1;      // 最大循环次数
    QString loopUntilImage;     // 循环终止条件图片
    QString description;        // 步骤描述(用于显示)
    QString failReason;         // 失败原因(用于EndFail)

    // JSON序列化
    static TaskStep fromJson(const QJsonObject& json);
    QJsonObject toJson() const;

    // 获取显示名称
    QString displayName() const;
};

// 任务定义
struct TaskDefinition {
    QString name;               // 任务名称
    QString description;        // 任务描述
    QString imageFolder;        // 图片文件夹名(相对路径)
    int version = 1;            // 版本号
    QList<TaskStep> steps;      // 步骤列表

    // JSON序列化
    static TaskDefinition fromJson(const QJsonObject& json);
    QJsonObject toJson() const;

    // 文件操作
    bool saveToFile(const QString& path) const;
    static TaskDefinition loadFromFile(const QString& path);

    // 验证任务定义是否有效
    bool isValid(QString* errorMsg = nullptr) const;

    // 获取所有引用的图片文件
    QStringList getReferencedImages() const;
};

// 任务包管理(导入/导出)
class TaskPackage {
public:
    // 导出任务包 (.hjdz文件)
    static bool exportTask(const TaskDefinition& task,
                          const QString& imageFolder,
                          const QString& outputPath);

    // 导入任务包
    static bool importTask(const QString& packagePath,
                          const QString& targetTaskDir,
                          const QString& targetImageDir,
                          TaskDefinition* outTask = nullptr);
};

Q_DECLARE_METATYPE(TaskDefinition)

#endif // TASKMODEL_H
