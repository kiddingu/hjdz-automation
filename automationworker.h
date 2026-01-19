#pragma once
#include <QObject>
#include <QImage>
#include <QPointer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QDir>
#include <QSharedPointer>
#include <QStringList>
#include <atomic>

class QWebEngineView;
struct StopToken;
class AWToolbox;
class ScriptRunner;
struct TaskDefinition;
class AutomationWorker : public QObject
{
    Q_OBJECT
public:
    explicit AutomationWorker(QWebEngineView* view,
                              QSharedPointer<StopToken> stop,
                              QObject* parent = nullptr);
    ~AutomationWorker();

signals:
    void log(const QString& msg);
    void finished(const QString& planName);   // 正常完成
    void aborted(const QString& reason);      // 外部打断/致命失败
    void logThumb(const QString& action, const QStringList& paths, double thr, int ms, double scale);

public slots:
    // 统一入口：根据 planName 选择一条任务序列并执行
    void runTask(const QString& planName);

    // 执行脚本任务
    void runScriptTask(const TaskDefinition& task);

public:
    // === 基础能力 ===
    bool shouldStop(const char* where) const;       // GUI 线程截图
    bool clickAt(const QPoint& localPos);               // GUI 线程点击（左键）
    static void sleepMs(int ms);
    QImage capture();
    QPoint findTemplatePlaceholder(const QImage& img,
                                   const QString& templatePng,
                                   double* outScore,
                                   double threshold);
    QString saveScreenshot(const QString& dir, const QString& tag);
    bool returnToHome(int maxMs = 8000);
private:
    QPointer<QWebEngineView> view_;
    QSharedPointer<StopToken> stop_;
    std::unique_ptr<AWToolbox> toolbox_;
    std::unique_ptr<ScriptRunner> scriptRunner_;


    bool runTask_NationalContest();
    bool runTask_WorldContest();
    bool runTask_AnnihilateGeneral();
    bool runTask_ArmamentSynthesis();
    bool runTask_NationalWar();
    bool runTask_Campaign();

    // 日常任务
    bool runTask_HeroCenter();
    bool runTask_WarAcademy();
    bool runTask_NationalTreasure();
    bool runTask_GeneralLottery();
    bool runTask_AdvisorLottery();
    bool runTask_ArtilleryLottery();
    bool runTask_AccessoryLottery();
    bool runTask_HeroSkill();
    bool runTask_OrePlunder();
    bool runTask_MonthlyCard();
    bool runTask_RechargeEvent();
    bool runTask_DailySignIn();
    bool runTask_WeeklyTasks();
    bool runTask_PublicCollection();
    bool runTask_GuildBattle();

    // 特色功能
    bool runTask_CardNationalWar();
};
