#ifndef AUTOMATIONPANEL_H
#define AUTOMATIONPANEL_H

#include <QDialog>
#include <QMap>
#include <QPushButton>

class AutomationPanel : public QDialog {
    Q_OBJECT
public:
    explicit AutomationPanel(QWidget *parent = nullptr);

signals:
    // 单项任务按钮，例如 "国家争霸"、"英雄中心" 等
    void requestTask(const QString &taskName);
    // 预设执行：1~4
    void requestPreset(int presetIndex);
    // 特色功能
    void requestSpecial(const QString &name);
    // 统一给外部用的信号
    void commandChosen(const QString& planName);

private:
    // 工具：批量创建按钮并加入布局
    QWidget* makeAutoBattleGroup();
    QWidget* makeDailyTasksGroup();
    QWidget* makePresetGroup();
    QWidget* makeSpecialGroup();

    QPushButton* makeBtn(const QString &text, const char *slotConn = nullptr);

    // 让同组按钮同尺寸
    void unifyButtonSizes(const QList<QPushButton*> &btns);

private slots:
    void onTaskClicked();          // 通用任务按钮
    void onPresetClicked();        // 预设按钮
    void onSpecialClicked();       // 特色功能按钮
};

#endif // AUTOMATIONPANEL_H
