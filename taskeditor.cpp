#include "taskeditor.h"
#include "stepwidget.h"
#include "screencapture.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QTextEdit>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QGroupBox>
#include <QScrollArea>
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QPushButton>

TaskEditor::TaskEditor(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("任务编辑器"));
    resize(1200, 800);

    setupUI();
    setupMenus();
    setupToolBar();

    loadTaskList();
}

TaskEditor::~TaskEditor() {
    if (screenCapture_) {
        delete screenCapture_;
    }
}

void TaskEditor::setGameView(QWebEngineView* view) {
    gameView_ = view;
}

void TaskEditor::setupUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 主分割器
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);

    // 左侧：任务列表
    QGroupBox* taskGroup = new QGroupBox(QStringLiteral("任务列表"), this);
    QVBoxLayout* taskLayout = new QVBoxLayout(taskGroup);
    taskList_ = new QListWidget(this);
    taskList_->setMinimumWidth(150);
    taskList_->setMaximumWidth(200);
    connect(taskList_, &QListWidget::itemClicked,
            this, &TaskEditor::onTaskSelected);
    taskLayout->addWidget(taskList_);
    mainSplitter->addWidget(taskGroup);

    // 中间：步骤列表
    QGroupBox* stepGroup = new QGroupBox(QStringLiteral("步骤列表"), this);
    QVBoxLayout* stepGroupLayout = new QVBoxLayout(stepGroup);

    QScrollArea* stepScroll = new QScrollArea(this);
    stepScroll->setWidgetResizable(true);
    stepScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    stepListWidget_ = new QWidget(this);
    stepListLayout_ = new QVBoxLayout(stepListWidget_);
    stepListLayout_->setContentsMargins(4, 4, 4, 4);
    stepListLayout_->setSpacing(4);
    stepListLayout_->addStretch();

    stepScroll->setWidget(stepListWidget_);
    stepGroupLayout->addWidget(stepScroll);

    // 步骤操作按钮
    QHBoxLayout* stepBtnLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton(QStringLiteral("+ 添加"), this);
    connect(addBtn, &QPushButton::clicked, this, &TaskEditor::onAddStep);
    stepBtnLayout->addWidget(addBtn);

    QPushButton* delBtn = new QPushButton(QStringLiteral("- 删除"), this);
    connect(delBtn, &QPushButton::clicked, this, &TaskEditor::onDeleteStep);
    stepBtnLayout->addWidget(delBtn);

    QPushButton* upBtn = new QPushButton(QStringLiteral("↑"), this);
    upBtn->setMaximumWidth(30);
    connect(upBtn, &QPushButton::clicked, this, &TaskEditor::onMoveStepUp);
    stepBtnLayout->addWidget(upBtn);

    QPushButton* downBtn = new QPushButton(QStringLiteral("↓"), this);
    downBtn->setMaximumWidth(30);
    connect(downBtn, &QPushButton::clicked, this, &TaskEditor::onMoveStepDown);
    stepBtnLayout->addWidget(downBtn);

    QPushButton* dupBtn = new QPushButton(QStringLiteral("复制"), this);
    connect(dupBtn, &QPushButton::clicked, this, &TaskEditor::onDuplicateStep);
    stepBtnLayout->addWidget(dupBtn);

    stepBtnLayout->addStretch();
    stepGroupLayout->addLayout(stepBtnLayout);

    mainSplitter->addWidget(stepGroup);

    // 右侧：属性面板
    QGroupBox* propGroup = new QGroupBox(QStringLiteral("步骤属性"), this);
    QVBoxLayout* propLayout = new QVBoxLayout(propGroup);

    QScrollArea* propScroll = new QScrollArea(this);
    propScroll->setWidgetResizable(true);
    propScroll->setMinimumWidth(280);

    propertyPanel_ = new StepPropertyPanel(this);
    connect(propertyPanel_, &StepPropertyPanel::stepChanged,
            this, &TaskEditor::onStepPropertyChanged);
    connect(propertyPanel_, &StepPropertyPanel::captureRequested,
            this, &TaskEditor::onCaptureImage);
    connect(propertyPanel_, &StepPropertyPanel::selectImageRequested,
            this, &TaskEditor::onSelectImage);

    propScroll->setWidget(propertyPanel_);
    propLayout->addWidget(propScroll);
    mainSplitter->addWidget(propGroup);

    // 设置分割比例
    mainSplitter->setStretchFactor(0, 1);  // 任务列表
    mainSplitter->setStretchFactor(1, 3);  // 步骤列表
    mainSplitter->setStretchFactor(2, 2);  // 属性面板

    mainLayout->addWidget(mainSplitter, 1);

    // 底部：日志输出
    QGroupBox* logGroup = new QGroupBox(QStringLiteral("日志"), this);
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    logOutput_ = new QTextEdit(this);
    logOutput_->setReadOnly(true);
    logOutput_->setMaximumHeight(120);
    logLayout->addWidget(logOutput_);
    mainLayout->addWidget(logGroup);
}

void TaskEditor::setupMenus() {
    // 文件菜单
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

    fileMenu->addAction(QStringLiteral("新建任务(&N)"), this, &TaskEditor::onNewTask,
                        QKeySequence::New);
    fileMenu->addAction(QStringLiteral("打开任务(&O)"), this, &TaskEditor::onOpenTask,
                        QKeySequence::Open);
    fileMenu->addAction(QStringLiteral("保存任务(&S)"), this, &TaskEditor::onSaveTask,
                        QKeySequence::Save);
    fileMenu->addAction(QStringLiteral("另存为..."), this, &TaskEditor::onSaveTaskAs);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("导入任务包(.hjdz)..."), this, &TaskEditor::onImportPackage);
    fileMenu->addAction(QStringLiteral("导出任务包(.hjdz)..."), this, &TaskEditor::onExportPackage);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出(&X)"), this, &QWidget::close);

    // 编辑菜单
    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    editMenu->addAction(QStringLiteral("添加步骤"), this, &TaskEditor::onAddStep);
    editMenu->addAction(QStringLiteral("删除步骤"), this, &TaskEditor::onDeleteStep,
                        QKeySequence::Delete);
    editMenu->addAction(QStringLiteral("上移步骤"), this, &TaskEditor::onMoveStepUp);
    editMenu->addAction(QStringLiteral("下移步骤"), this, &TaskEditor::onMoveStepDown);
    editMenu->addAction(QStringLiteral("复制步骤"), this, &TaskEditor::onDuplicateStep);

    // 运行菜单
    QMenu* runMenu = menuBar()->addMenu(QStringLiteral("运行(&R)"));
    runMenu->addAction(QStringLiteral("测试运行(&T)"), this, &TaskEditor::onTestRun,
                       QKeySequence(Qt::Key_F5));
}

void TaskEditor::setupToolBar() {
    QToolBar* toolbar = addToolBar(QStringLiteral("工具栏"));

    toolbar->addAction(QStringLiteral("新建"), this, &TaskEditor::onNewTask);
    toolbar->addAction(QStringLiteral("保存"), this, &TaskEditor::onSaveTask);
    toolbar->addSeparator();
    toolbar->addAction(QStringLiteral("截图"), this, &TaskEditor::onCaptureImage);
    toolbar->addSeparator();
    toolbar->addAction(QStringLiteral("测试运行"), this, &TaskEditor::onTestRun);
}

QString TaskEditor::getTaskDir() const {
    return QApplication::applicationDirPath() + "/custom_tasks";
}

QString TaskEditor::getImageDir() const {
    return QApplication::applicationDirPath() + QStringLiteral("/游戏图片/自定义");
}

void TaskEditor::loadTaskList() {
    taskList_->clear();

    QDir dir(getTaskDir());
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const auto& file : files) {
        QListWidgetItem* item = new QListWidgetItem(file.baseName());
        item->setData(Qt::UserRole, file.absoluteFilePath());
        taskList_->addItem(item);
    }
}

void TaskEditor::onNewTask() {
    bool ok;
    QString name = QInputDialog::getText(this, QStringLiteral("新建任务"),
                                         QStringLiteral("任务名称:"),
                                         QLineEdit::Normal, QString(), &ok);
    if (!ok || name.isEmpty()) return;

    // 检查是否已存在
    QString path = getTaskDir() + "/" + name + ".json";
    if (QFile::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("任务 '%1' 已存在").arg(name));
        return;
    }

    // 创建新任务
    currentTask_ = TaskDefinition();
    currentTask_.name = name;
    currentTask_.imageFolder = QStringLiteral("游戏图片/自定义/") + name;
    currentTaskPath_ = path;
    modified_ = true;

    // 创建图片目录
    QDir().mkpath(getImageDir() + "/" + name);

    refreshStepList();
    propertyPanel_->clear();

    appendLog(QStringLiteral("新建任务: %1").arg(name));

    // 保存并刷新列表
    saveCurrentTask();
    loadTaskList();
}

void TaskEditor::onOpenTask() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开任务"),
                                                 getTaskDir(),
                                                 QStringLiteral("任务文件 (*.json)"));
    if (path.isEmpty()) return;
    loadTask(path);
}

void TaskEditor::onSaveTask() {
    if (currentTask_.name.isEmpty()) {
        onSaveTaskAs();
        return;
    }
    saveCurrentTask();
}

void TaskEditor::onSaveTaskAs() {
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存任务"),
                                                 getTaskDir() + "/" + currentTask_.name + ".json",
                                                 QStringLiteral("任务文件 (*.json)"));
    if (path.isEmpty()) return;

    currentTaskPath_ = path;
    currentTask_.name = QFileInfo(path).baseName();
    saveCurrentTask();
    loadTaskList();
}

void TaskEditor::onImportPackage() {
    QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("导入任务包"),
                                                       QString());
    if (path.isEmpty()) return;

    TaskDefinition task;
    if (TaskPackage::importTask(path, getTaskDir(), getImageDir(), &task)) {
        appendLog(QStringLiteral("导入成功: %1").arg(task.name));
        loadTaskList();
        loadTask(getTaskDir() + "/" + task.name + ".json");
    } else {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("导入失败"));
    }
}

void TaskEditor::onExportPackage() {
    if (currentTask_.name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("请先打开一个任务"));
        return;
    }

    QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("导出到目录"),
                                                       QString());
    if (path.isEmpty()) return;

    QString imgFolder = getImageDir() + "/" + currentTask_.name;
    if (TaskPackage::exportTask(currentTask_, imgFolder, path + "/" + currentTask_.name)) {
        appendLog(QStringLiteral("导出成功: %1").arg(path));
        QMessageBox::information(this, QStringLiteral("成功"),
                                 QStringLiteral("任务已导出到: %1").arg(path));
    } else {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("导出失败"));
    }
}

void TaskEditor::onTaskSelected(QListWidgetItem* item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    loadTask(path);
}

void TaskEditor::loadTask(const QString& path) {
    currentTask_ = TaskDefinition::loadFromFile(path);
    currentTaskPath_ = path;
    modified_ = false;

    setWindowTitle(QStringLiteral("任务编辑器 - %1").arg(currentTask_.name));
    refreshStepList();
    propertyPanel_->clear();
    selectedStepIndex_ = -1;

    appendLog(QStringLiteral("加载任务: %1 (共 %2 步)")
                  .arg(currentTask_.name)
                  .arg(currentTask_.steps.size()));
}

void TaskEditor::saveCurrentTask() {
    if (currentTaskPath_.isEmpty()) return;

    QDir().mkpath(QFileInfo(currentTaskPath_).absolutePath());
    if (currentTask_.saveToFile(currentTaskPath_)) {
        modified_ = false;
        appendLog(QStringLiteral("保存成功: %1").arg(currentTaskPath_));
    } else {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("保存失败"));
    }
}

void TaskEditor::refreshStepList() {
    // 清除旧的步骤项
    for (auto* item : stepItems_) {
        stepListLayout_->removeWidget(item);
        delete item;
    }
    stepItems_.clear();

    // 添加新的步骤项
    for (int i = 0; i < currentTask_.steps.size(); ++i) {
        StepListItem* item = new StepListItem(currentTask_.steps[i], i, stepListWidget_);
        connect(item, &StepListItem::clicked, this, &TaskEditor::onStepClicked);

        stepListLayout_->insertWidget(stepListLayout_->count() - 1, item);
        stepItems_.append(item);
    }
}

void TaskEditor::selectStep(int index) {
    // 取消之前的选择
    if (selectedStepIndex_ >= 0 && selectedStepIndex_ < stepItems_.size()) {
        stepItems_[selectedStepIndex_]->setSelected(false);
    }

    selectedStepIndex_ = index;

    if (index >= 0 && index < stepItems_.size()) {
        stepItems_[index]->setSelected(true);
        propertyPanel_->setStep(currentTask_.steps[index]);
    } else {
        propertyPanel_->clear();
    }
}

void TaskEditor::updateStepItem(int index) {
    if (index >= 0 && index < stepItems_.size() && index < currentTask_.steps.size()) {
        stepItems_[index]->setStep(currentTask_.steps[index]);
    }
}

void TaskEditor::onAddStep() {
    TaskStep newStep;
    newStep.type = StepType::WaitClick;
    newStep.id = QString("step%1").arg(currentTask_.steps.size() + 1);

    int insertIndex = selectedStepIndex_ >= 0 ? selectedStepIndex_ + 1 : currentTask_.steps.size();
    currentTask_.steps.insert(insertIndex, newStep);

    modified_ = true;
    refreshStepList();
    selectStep(insertIndex);

    appendLog(QStringLiteral("添加步骤: %1").arg(newStep.id));
}

void TaskEditor::onDeleteStep() {
    if (selectedStepIndex_ < 0 || selectedStepIndex_ >= currentTask_.steps.size()) return;

    if (QMessageBox::question(this, QStringLiteral("确认"),
                              QStringLiteral("确定要删除这个步骤吗?")) != QMessageBox::Yes) {
        return;
    }

    currentTask_.steps.removeAt(selectedStepIndex_);
    modified_ = true;
    refreshStepList();

    if (selectedStepIndex_ >= currentTask_.steps.size()) {
        selectedStepIndex_ = currentTask_.steps.size() - 1;
    }
    selectStep(selectedStepIndex_);
}

void TaskEditor::onMoveStepUp() {
    if (selectedStepIndex_ <= 0) return;

    currentTask_.steps.swapItemsAt(selectedStepIndex_, selectedStepIndex_ - 1);
    modified_ = true;
    refreshStepList();
    selectStep(selectedStepIndex_ - 1);
}

void TaskEditor::onMoveStepDown() {
    if (selectedStepIndex_ < 0 || selectedStepIndex_ >= currentTask_.steps.size() - 1) return;

    currentTask_.steps.swapItemsAt(selectedStepIndex_, selectedStepIndex_ + 1);
    modified_ = true;
    refreshStepList();
    selectStep(selectedStepIndex_ + 1);
}

void TaskEditor::onDuplicateStep() {
    if (selectedStepIndex_ < 0 || selectedStepIndex_ >= currentTask_.steps.size()) return;

    TaskStep copy = currentTask_.steps[selectedStepIndex_];
    copy.id = copy.id + "_copy";

    currentTask_.steps.insert(selectedStepIndex_ + 1, copy);
    modified_ = true;
    refreshStepList();
    selectStep(selectedStepIndex_ + 1);
}

void TaskEditor::onStepClicked(int index) {
    selectStep(index);
}

void TaskEditor::onStepPropertyChanged(const TaskStep& step) {
    if (selectedStepIndex_ < 0 || selectedStepIndex_ >= currentTask_.steps.size()) return;

    currentTask_.steps[selectedStepIndex_] = step;
    modified_ = true;
    updateStepItem(selectedStepIndex_);
}

void TaskEditor::onCaptureImage() {
    if (!gameView_) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("没有可用的游戏窗口"));
        return;
    }

    if (!screenCapture_) {
        screenCapture_ = new ScreenCapture();
        connect(screenCapture_, &ScreenCapture::captured,
                this, &TaskEditor::onImageCaptured);
    }

    screenCapture_->setGameView(gameView_);
    screenCapture_->startCapture();
}

void TaskEditor::onSelectImage() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择图片"),
                                                 getImageDir(),
                                                 QStringLiteral("图片文件 (*.png *.jpg *.bmp)"));
    if (path.isEmpty()) return;

    // 添加图片到当前步骤
    if (selectedStepIndex_ >= 0 && selectedStepIndex_ < currentTask_.steps.size()) {
        QString relativePath = QFileInfo(path).fileName();
        currentTask_.steps[selectedStepIndex_].images << relativePath;
        modified_ = true;
        propertyPanel_->setStep(currentTask_.steps[selectedStepIndex_]);
        updateStepItem(selectedStepIndex_);
        appendLog(QStringLiteral("添加图片: %1").arg(relativePath));
    }
}

void TaskEditor::onImageCaptured(const QImage& image, const QRect& region) {
    Q_UNUSED(region)

    // 显示保存对话框
    ScreenCaptureDialog* dialog = new ScreenCaptureDialog(image, this);
    connect(dialog, &ScreenCaptureDialog::accepted, [this](const QString& fileName, const QImage& img) {
        // 保存图片
        QString dir = getImageDir();
        if (!currentTask_.name.isEmpty()) {
            dir += "/" + currentTask_.name;
        }
        QDir().mkpath(dir);

        QString path = dir + "/" + fileName + ".png";
        if (img.save(path)) {
            // 添加图片到当前步骤
            if (selectedStepIndex_ >= 0 && selectedStepIndex_ < currentTask_.steps.size()) {
                currentTask_.steps[selectedStepIndex_].images << (fileName + ".png");
                modified_ = true;
                propertyPanel_->setStep(currentTask_.steps[selectedStepIndex_]);
                updateStepItem(selectedStepIndex_);
            }
            appendLog(QStringLiteral("截图已保存: %1").arg(path));
        }
    });

    dialog->show();
}

void TaskEditor::onTestRun() {
    if (currentTask_.steps.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("任务没有步骤"));
        return;
    }

    appendLog(QStringLiteral("请求测试运行: %1").arg(currentTask_.name));
    emit runTaskRequested(currentTask_);
}

void TaskEditor::appendLog(const QString& msg) {
    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss] ");
    logOutput_->append(timestamp + msg);
}
