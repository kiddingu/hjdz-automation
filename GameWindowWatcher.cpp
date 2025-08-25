#include "GameWindowWatcher.h"
#include <QWebEngineView>
#include <QTimer>
#include <QDateTime>

static void appendLog(QTextEdit *log, const QString &text) {
    if (!log) return;
    log->append("[" + QDateTime::currentDateTime().toString("hh:mm:ss") + "] " + text);
}

GameWindowWatcher::GameWindowWatcher(QTabWidget *tabWidget, QTextEdit *logWidget, QObject *parent)
    : QObject(parent), tabWidget(tabWidget), logWidget(logWidget)
{
    // 可选：心跳定时器，用于占位“自动检测”逻辑（你可以替换为图像识别等）
    heartbeat = new QTimer(this);
    heartbeat->setInterval(10000); // 10 秒
    connect(heartbeat, &QTimer::timeout, this, [this]() {
        appendLog(this->logWidget, QStringLiteral("[心跳] 正在监控游戏窗口..."));
        // TODO: 在这里调用你的视觉检测/自动点击逻辑
        // performVisionCheck(targetView);
    });
}
void GameWindowWatcher::start()
{
    if (heartbeat && !heartbeat->isActive()) heartbeat->start();
}

void GameWindowWatcher::stop()
{
    if (heartbeat && heartbeat->isActive()) heartbeat->stop();
}

void GameWindowWatcher::setTargetView(QWebEngineView *view)
{
    targetView = view;
}

bool GameWindowWatcher::eventFilter(QObject *obj, QEvent *event) {
    if (obj != targetView) return QObject::eventFilter(obj, event);

    switch (event->type()) {
    case QEvent::Close:
    case QEvent::Destroy:
    case QEvent::DeferredDelete: {
        qDebug()<<"watcher deleting";
        stop();
        if (tabWidget) {
            QWidget *page = tabPage ? tabPage.data()
                                    : (logWidget ? logWidget->parentWidget() : nullptr);
            int idx = page ? tabWidget->indexOf(page) : -1;
            if (idx != -1) {
                qDebug()<<"Watcher remove tab";
                QWidget *w = tabWidget->widget(idx);
                tabWidget->removeTab(idx);
                if (w) w->deleteLater();
            }
        }
        break;
    }
    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

