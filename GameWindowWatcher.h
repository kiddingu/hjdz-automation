#ifndef GAMEWINDOWWATCHER_H
#define GAMEWINDOWWATCHER_H

#pragma once
#include <QObject>
#include <QPointer>
#include <QTabWidget>   // ✅ 必须加，否则 QPointer 无法实例化
#include <QTextEdit>
#include <QWebEngineView>
#include <QTimer>
#include <Qevent>

class GameWindowWatcher : public QObject {
    Q_OBJECT
public:
    explicit GameWindowWatcher(QTabWidget *tabWidget, QTextEdit *logWidget, QObject *parent=nullptr);
    void setTargetView(QWebEngineView *view);
    void setTabPage(QWidget *page) { tabPage = page; }
    void start();
    void stop();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QPointer<QTabWidget> tabWidget;
    QPointer<QTextEdit>  logWidget;
    QPointer<QWebEngineView> targetView;
    QPointer<QWidget> tabPage;   // 新增：实际加入 QTabWidget 的容器页
    QTimer *heartbeat = nullptr;
};


#endif // GAMEWINDOWWATCHER_H
