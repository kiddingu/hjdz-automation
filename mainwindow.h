#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QTableWidget>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineCookieStore>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTabWidget>
#include <QMap>
#include <QTextEdit>
#include <QHash>
#include <QThread>
#include <QPointer>
#include <QVector>
#include <StopToken.h>
class QWebEngineView;
class QTextEdit;
class QWidget;
class GameWindowWatcher;
class AutomationWorker; // 前置声明

struct GameWindowCtx {
    QPointer<QWebEngineView>  view{};     // 该游戏窗口
    QPointer<QWidget>         tab{};      // 日志页容器
    QPointer<QTextEdit>       log{};      // 日志框
    QSharedPointer<StopToken> stop{};     // 停止信号
    QThread*                  thread{};   // 跑 worker 的子线程
    AutomationWorker*         worker{};   // 执行业务逻辑（仍可阻塞式循环）
    bool                      active{};   // 是否有任务在跑
    QDateTime                 lastActive; // 可选：最近活跃，用于“闲时回收”
};
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void openGameDialog();  // 弹出添加QQ账号对话框
    void openQQDialog();  // 弹出添加QQ账号对话框
    void openRedAlertDialog();
    void onRunAll();      // 全部执行 —— 打开命令面板
    void onStopAll();     // 全部停止
    void onRefreshAll();  // 全部刷新
    void onExecuteAll(const QString& planName); // 命令面板回调：对所有窗口执行

    void registerGameWindow(QWebEngineView* view, QTextEdit* log, QWidget* tab);
    void unregisterGameWindow(QWebEngineView* view);

    void startAutomationFor(QWebEngineView* view, QTextEdit* log, const QString& planName);
    void stopAutomationFor(QWebEngineView* view, QTextEdit* log = nullptr);



private:

    void setStatus(QTableWidget *table, int row, const QString &statusText);
    void saveRedAccountsToFile(QTableWidget *raTable);
    void setupOperationButtons(QTableWidget *table, int row);
    void saveQQAccountsToFile(QTableWidget *qqTable);
    void setupRAOperationButtons(QTableWidget *table, int row);
    void emitLogToGameTab(QTextEdit *tab, const QString &text);
    void startAutomationFor(QWebEngineView *view, QTextEdit *log);

    QTabWidget *logTabWidget;
    QMap<QString, QTextEdit*> perWindowLogs;
    QTextEdit *logOutput;            // 单账号日志
    QTextEdit *globalLogOutput;      // 全局日志
    QTableWidget *qqTable;           // QQ账号表（不显示，但用于管理账号）
    QNetworkAccessManager *manager;  // 用于检测URL状态
    QWebEngineView *webView = nullptr;   // QQ登录用浏览器视图
    QDialog *loginDialog = nullptr;      // 登录弹窗
    QVector<GameWindowCtx> windows_;
    QHash<QWebEngineView*, GameWindowCtx> gameWindows_;
    QHash<QString, QWebEngineProfile*> profiles_; // key: qq号
    QWebEngineProfile* profileForQQ(const QString& qq);

    GameWindowCtx* ctxFor(QWebEngineView* v);
    GameWindowCtx& ensureCtx(QWebEngineView* v); // 若无则创建空 ctx（仅存 view）
    void ensureThreadAndWorker(GameWindowCtx& ctx);     // 按需创建/连接/启动
    void teardownThreadAndWorker(GameWindowCtx& ctx);   // 回收线程/worker
    void removeTabIfAny(GameWindowCtx& ctx);
};

#endif // MAINWINDOW_H
