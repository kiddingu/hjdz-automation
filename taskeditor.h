#ifndef TASKEDITOR_H
#define TASKEDITOR_H

#include <QMainWindow>
#include <QVBoxLayout>
#include "taskmodel.h"

class QWebEngineView;
class QListWidget;
class QTextEdit;
class QListWidgetItem;
class StepPropertyPanel;
class ScreenCapture;
class QPushButton;

// 可视化任务编辑器
class TaskEditor : public QMainWindow {
    Q_OBJECT
public:
    explicit TaskEditor(QWidget* parent = nullptr);
    ~TaskEditor();

    // 设置游戏视图(用于截图)
    void setGameView(QWebEngineView* view);

    // 获取当前任务
    TaskDefinition getCurrentTask() const { return currentTask_; }

signals:
    // 请求执行任务
    void runTaskRequested(const TaskDefinition& task);

private slots:
    // 文件操作
    void onNewTask();
    void onOpenTask();
    void onSaveTask();
    void onSaveTaskAs();
    void onImportPackage();
    void onExportPackage();

    // 任务列表操作
    void onTaskSelected(QListWidgetItem* item);

    // 步骤操作
    void onAddStep();
    void onDeleteStep();
    void onMoveStepUp();
    void onMoveStepDown();
    void onDuplicateStep();
    void onStepClicked(int index);
    void onStepPropertyChanged(const TaskStep& step);

    // 截图操作
    void onCaptureImage();
    void onSelectImage();
    void onImageCaptured(const QImage& image, const QRect& region);

    // 运行操作
    void onTestRun();

    // 日志
    void appendLog(const QString& msg);

private:
    void setupUI();
    void setupMenus();
    void setupToolBar();

    void loadTaskList();
    void saveCurrentTask();
    void loadTask(const QString& path);

    void refreshStepList();
    void selectStep(int index);
    void updateStepItem(int index);

    QString getTaskDir() const;
    QString getImageDir() const;

    // UI 组件
    QListWidget* taskList_;          // 左侧任务列表
    QWidget* stepListWidget_;        // 中间步骤列表容器
    QVBoxLayout* stepListLayout_;    // 步骤列表布局
    QList<class StepListItem*> stepItems_;  // 步骤项列表
    StepPropertyPanel* propertyPanel_;  // 右侧属性面板
    QTextEdit* logOutput_;           // 底部日志

    // 数据
    TaskDefinition currentTask_;
    QString currentTaskPath_;
    int selectedStepIndex_ = -1;
    bool modified_ = false;

    // 截图
    QWebEngineView* gameView_ = nullptr;
    ScreenCapture* screenCapture_ = nullptr;
};

#endif // TASKEDITOR_H
