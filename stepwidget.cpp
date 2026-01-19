#include "stepwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QGroupBox>
#include <QScrollArea>
#include <QPainter>
#include <QMouseEvent>
#include <QFileInfo>

// ============== StepListItem ==============

StepListItem::StepListItem(const TaskStep& step, int index, QWidget* parent)
    : QFrame(parent), step_(step), index_(index)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setFixedHeight(50);
    setCursor(Qt::PointingHandCursor);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    iconLabel_ = new QLabel(this);
    iconLabel_->setFixedSize(32, 32);
    iconLabel_->setAlignment(Qt::AlignCenter);
    iconLabel_->setStyleSheet("QLabel { background: #444; border-radius: 4px; color: white; font-weight: bold; }");
    layout->addWidget(iconLabel_);

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);

    nameLabel_ = new QLabel(this);
    nameLabel_->setStyleSheet("QLabel { font-weight: bold; }");
    textLayout->addWidget(nameLabel_);

    detailLabel_ = new QLabel(this);
    detailLabel_->setStyleSheet("QLabel { color: #888; font-size: 11px; }");
    textLayout->addWidget(detailLabel_);

    layout->addLayout(textLayout, 1);

    updateDisplay();
}

void StepListItem::setStep(const TaskStep& step) {
    step_ = step;
    updateDisplay();
}

void StepListItem::setIndex(int index) {
    index_ = index;
    updateDisplay();
}

void StepListItem::setSelected(bool selected) {
    selected_ = selected;
    update();
}

QString StepListItem::getTypeIcon() const {
    switch (step_.type) {
        case StepType::WaitClick:     return QStringLiteral("W+C");
        case StepType::WaitAppear:    return QStringLiteral("W");
        case StepType::WaitDisappear: return QStringLiteral("W-");
        case StepType::Click:         return QStringLiteral("C");
        case StepType::ClickPos:      return QStringLiteral("XY");
        case StepType::Sleep:         return QStringLiteral("Z");
        case StepType::IfExist:       return QStringLiteral("IF");
        case StepType::IfExistClick:  return QStringLiteral("IF+C");
        case StepType::Loop:          return QStringLiteral("L");
        case StepType::LoopUntil:     return QStringLiteral("LU");
        case StepType::Goto:          return QStringLiteral("GO");
        case StepType::EndSuccess:    return QStringLiteral("OK");
        case StepType::EndFail:       return QStringLiteral("X");
    }
    return "?";
}

void StepListItem::updateDisplay() {
    iconLabel_->setText(getTypeIcon());
    nameLabel_->setText(QString("%1. %2").arg(index_ + 1).arg(step_.displayName()));

    // 详细信息
    QStringList details;
    if (!step_.images.isEmpty()) {
        details << QStringLiteral("图片: %1").arg(step_.images.join(", "));
    }
    if (step_.timeout > 0 && step_.type != StepType::Sleep) {
        details << QStringLiteral("超时: %1ms").arg(step_.timeout);
    }
    if (step_.sleepMs > 0) {
        details << QStringLiteral("延迟: %1ms").arg(step_.sleepMs);
    }
    detailLabel_->setText(details.join(" | "));
}

void StepListItem::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(index_);
    }
    QFrame::mousePressEvent(event);
}

void StepListItem::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit doubleClicked(index_);
    }
    QFrame::mouseDoubleClickEvent(event);
}

void StepListItem::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);

    if (selected_) {
        QPainter painter(this);
        painter.setPen(QPen(QColor(0, 120, 215), 2));
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }
}

// ============== StepPropertyPanel ==============

StepPropertyPanel::StepPropertyPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void StepPropertyPanel::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 基本属性组
    QGroupBox* basicGroup = new QGroupBox(QStringLiteral("基本属性"), this);
    QGridLayout* basicLayout = new QGridLayout(basicGroup);

    basicLayout->addWidget(new QLabel(QStringLiteral("类型:"), this), 0, 0);
    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem(QStringLiteral("等待点击"), static_cast<int>(StepType::WaitClick));
    typeCombo_->addItem(QStringLiteral("等待出现"), static_cast<int>(StepType::WaitAppear));
    typeCombo_->addItem(QStringLiteral("等待消失"), static_cast<int>(StepType::WaitDisappear));
    typeCombo_->addItem(QStringLiteral("点击"), static_cast<int>(StepType::Click));
    typeCombo_->addItem(QStringLiteral("点击坐标"), static_cast<int>(StepType::ClickPos));
    typeCombo_->addItem(QStringLiteral("延迟"), static_cast<int>(StepType::Sleep));
    typeCombo_->addItem(QStringLiteral("条件判断"), static_cast<int>(StepType::IfExist));
    typeCombo_->addItem(QStringLiteral("存在则点"), static_cast<int>(StepType::IfExistClick));
    typeCombo_->addItem(QStringLiteral("循环"), static_cast<int>(StepType::Loop));
    typeCombo_->addItem(QStringLiteral("循环直到"), static_cast<int>(StepType::LoopUntil));
    typeCombo_->addItem(QStringLiteral("跳转"), static_cast<int>(StepType::Goto));
    typeCombo_->addItem(QStringLiteral("成功结束"), static_cast<int>(StepType::EndSuccess));
    typeCombo_->addItem(QStringLiteral("失败结束"), static_cast<int>(StepType::EndFail));
    connect(typeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StepPropertyPanel::onTypeChanged);
    basicLayout->addWidget(typeCombo_, 0, 1);

    basicLayout->addWidget(new QLabel(QStringLiteral("步骤ID:"), this), 1, 0);
    idEdit_ = new QLineEdit(this);
    idEdit_->setPlaceholderText(QStringLiteral("用于跳转引用"));
    connect(idEdit_, &QLineEdit::textChanged, this, &StepPropertyPanel::onPropertyChanged);
    basicLayout->addWidget(idEdit_, 1, 1);

    basicLayout->addWidget(new QLabel(QStringLiteral("描述:"), this), 2, 0);
    descEdit_ = new QLineEdit(this);
    descEdit_->setPlaceholderText(QStringLiteral("步骤描述(可选)"));
    connect(descEdit_, &QLineEdit::textChanged, this, &StepPropertyPanel::onPropertyChanged);
    basicLayout->addWidget(descEdit_, 2, 1);

    mainLayout->addWidget(basicGroup);

    // 图片属性组
    imageGroup_ = new QGroupBox(QStringLiteral("图片匹配"), this);
    QVBoxLayout* imageLayout = new QVBoxLayout(imageGroup_);

    imageList_ = new QListWidget(this);
    imageList_->setMaximumHeight(80);
    imageLayout->addWidget(imageList_);

    QHBoxLayout* imgBtnLayout = new QHBoxLayout();
    captureBtn_ = new QPushButton(QStringLiteral("截图"), this);
    connect(captureBtn_, &QPushButton::clicked, this, &StepPropertyPanel::onCaptureImage);
    imgBtnLayout->addWidget(captureBtn_);

    selectBtn_ = new QPushButton(QStringLiteral("选择"), this);
    connect(selectBtn_, &QPushButton::clicked, this, &StepPropertyPanel::onSelectImage);
    imgBtnLayout->addWidget(selectBtn_);

    removeImageBtn_ = new QPushButton(QStringLiteral("删除"), this);
    connect(removeImageBtn_, &QPushButton::clicked, this, &StepPropertyPanel::onRemoveImage);
    imgBtnLayout->addWidget(removeImageBtn_);
    imageLayout->addLayout(imgBtnLayout);

    QHBoxLayout* matchLayout = new QHBoxLayout();
    matchLayout->addWidget(new QLabel(QStringLiteral("匹配:"), this));
    matchModeCombo_ = new QComboBox(this);
    matchModeCombo_->addItem(QStringLiteral("任意一个"), "any");
    matchModeCombo_->addItem(QStringLiteral("全部匹配"), "all");
    connect(matchModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &StepPropertyPanel::onPropertyChanged);
    matchLayout->addWidget(matchModeCombo_);

    matchLayout->addWidget(new QLabel(QStringLiteral("阈值:"), this));
    thresholdSpin_ = new QDoubleSpinBox(this);
    thresholdSpin_->setRange(0.5, 1.0);
    thresholdSpin_->setSingleStep(0.05);
    thresholdSpin_->setValue(0.85);
    connect(thresholdSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StepPropertyPanel::onPropertyChanged);
    matchLayout->addWidget(thresholdSpin_);
    imageLayout->addLayout(matchLayout);

    mainLayout->addWidget(imageGroup_);

    // 时间属性组
    timeGroup_ = new QGroupBox(QStringLiteral("时间设置"), this);
    QGridLayout* timeLayout = new QGridLayout(timeGroup_);

    timeLayout->addWidget(new QLabel(QStringLiteral("超时(ms):"), this), 0, 0);
    timeoutSpin_ = new QSpinBox(this);
    timeoutSpin_->setRange(0, 60000);
    timeoutSpin_->setSingleStep(1000);
    timeoutSpin_->setValue(8000);
    connect(timeoutSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StepPropertyPanel::onPropertyChanged);
    timeLayout->addWidget(timeoutSpin_, 0, 1);

    timeLayout->addWidget(new QLabel(QStringLiteral("延迟(ms):"), this), 1, 0);
    sleepSpin_ = new QSpinBox(this);
    sleepSpin_->setRange(0, 60000);
    sleepSpin_->setSingleStep(100);
    sleepSpin_->setValue(0);
    connect(sleepSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StepPropertyPanel::onPropertyChanged);
    timeLayout->addWidget(sleepSpin_, 1, 1);

    mainLayout->addWidget(timeGroup_);

    // 坐标属性组
    posGroup_ = new QGroupBox(QStringLiteral("坐标/偏移"), this);
    QHBoxLayout* posLayout = new QHBoxLayout(posGroup_);

    posLayout->addWidget(new QLabel(QStringLiteral("X:"), this));
    xSpin_ = new QSpinBox(this);
    xSpin_->setRange(-1000, 2000);
    connect(xSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StepPropertyPanel::onPropertyChanged);
    posLayout->addWidget(xSpin_);

    posLayout->addWidget(new QLabel(QStringLiteral("Y:"), this));
    ySpin_ = new QSpinBox(this);
    ySpin_->setRange(-1000, 2000);
    connect(ySpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StepPropertyPanel::onPropertyChanged);
    posLayout->addWidget(ySpin_);

    mainLayout->addWidget(posGroup_);

    // 跳转属性组
    gotoGroup_ = new QGroupBox(QStringLiteral("跳转设置"), this);
    QGridLayout* gotoLayout = new QGridLayout(gotoGroup_);

    gotoLayout->addWidget(new QLabel(QStringLiteral("成功后:"), this), 0, 0);
    onSuccessEdit_ = new QLineEdit(this);
    onSuccessEdit_->setPlaceholderText(QStringLiteral("步骤ID(留空则继续)"));
    connect(onSuccessEdit_, &QLineEdit::textChanged, this, &StepPropertyPanel::onPropertyChanged);
    gotoLayout->addWidget(onSuccessEdit_, 0, 1);

    gotoLayout->addWidget(new QLabel(QStringLiteral("失败后:"), this), 1, 0);
    onFailEdit_ = new QLineEdit(this);
    onFailEdit_->setPlaceholderText(QStringLiteral("步骤ID(留空则结束)"));
    connect(onFailEdit_, &QLineEdit::textChanged, this, &StepPropertyPanel::onPropertyChanged);
    gotoLayout->addWidget(onFailEdit_, 1, 1);

    mainLayout->addWidget(gotoGroup_);

    // 循环属性组
    loopGroup_ = new QGroupBox(QStringLiteral("循环设置"), this);
    QGridLayout* loopLayout = new QGridLayout(loopGroup_);

    loopLayout->addWidget(new QLabel(QStringLiteral("最大次数:"), this), 0, 0);
    maxIterSpin_ = new QSpinBox(this);
    maxIterSpin_->setRange(1, 1000);
    maxIterSpin_->setValue(1);
    connect(maxIterSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &StepPropertyPanel::onPropertyChanged);
    loopLayout->addWidget(maxIterSpin_, 0, 1);

    loopLayout->addWidget(new QLabel(QStringLiteral("终止图片:"), this), 1, 0);
    untilImageEdit_ = new QLineEdit(this);
    untilImageEdit_->setPlaceholderText(QStringLiteral("检测到此图片则退出"));
    connect(untilImageEdit_, &QLineEdit::textChanged, this, &StepPropertyPanel::onPropertyChanged);
    loopLayout->addWidget(untilImageEdit_, 1, 1);

    mainLayout->addWidget(loopGroup_);

    // 失败原因组
    failGroup_ = new QGroupBox(QStringLiteral("失败设置"), this);
    QHBoxLayout* failLayout = new QHBoxLayout(failGroup_);

    failLayout->addWidget(new QLabel(QStringLiteral("原因:"), this));
    failReasonEdit_ = new QLineEdit(this);
    failReasonEdit_->setPlaceholderText(QStringLiteral("失败时显示的原因"));
    connect(failReasonEdit_, &QLineEdit::textChanged, this, &StepPropertyPanel::onPropertyChanged);
    failLayout->addWidget(failReasonEdit_);

    mainLayout->addWidget(failGroup_);

    mainLayout->addStretch();

    // 初始隐藏所有可选组
    imageGroup_->hide();
    timeGroup_->hide();
    posGroup_->hide();
    gotoGroup_->hide();
    loopGroup_->hide();
    failGroup_->hide();
}

void StepPropertyPanel::setStep(const TaskStep& step) {
    updating_ = true;
    currentStep_ = step;

    // 设置类型
    for (int i = 0; i < typeCombo_->count(); ++i) {
        if (typeCombo_->itemData(i).toInt() == static_cast<int>(step.type)) {
            typeCombo_->setCurrentIndex(i);
            break;
        }
    }

    // 设置基本属性
    idEdit_->setText(step.id);
    descEdit_->setText(step.description);

    // 设置图片
    updateImageList();
    matchModeCombo_->setCurrentIndex(step.matchMode == "all" ? 1 : 0);
    thresholdSpin_->setValue(step.threshold);

    // 设置时间
    timeoutSpin_->setValue(step.timeout);
    sleepSpin_->setValue(step.sleepMs);

    // 设置坐标
    xSpin_->setValue(step.clickOffset.x());
    ySpin_->setValue(step.clickOffset.y());

    // 设置跳转
    onSuccessEdit_->setText(step.onSuccess);
    onFailEdit_->setText(step.onFail);

    // 设置循环
    maxIterSpin_->setValue(step.maxIterations);
    untilImageEdit_->setText(step.loopUntilImage);

    // 设置失败原因
    failReasonEdit_->setText(step.failReason);

    updateUIForType(step.type);
    updating_ = false;
}

TaskStep StepPropertyPanel::getStep() const {
    return currentStep_;
}

void StepPropertyPanel::clear() {
    updating_ = true;
    currentStep_ = TaskStep();
    idEdit_->clear();
    descEdit_->clear();
    imageList_->clear();
    timeoutSpin_->setValue(8000);
    sleepSpin_->setValue(0);
    xSpin_->setValue(0);
    ySpin_->setValue(0);
    onSuccessEdit_->clear();
    onFailEdit_->clear();
    maxIterSpin_->setValue(1);
    untilImageEdit_->clear();
    failReasonEdit_->clear();
    updating_ = false;
}

void StepPropertyPanel::onTypeChanged(int index) {
    if (updating_) return;

    StepType newType = static_cast<StepType>(typeCombo_->itemData(index).toInt());
    currentStep_.type = newType;
    updateUIForType(newType);
    emit stepChanged(currentStep_);
}

void StepPropertyPanel::onPropertyChanged() {
    if (updating_) return;

    currentStep_.id = idEdit_->text().trimmed();
    currentStep_.description = descEdit_->text().trimmed();
    currentStep_.matchMode = matchModeCombo_->currentData().toString();
    currentStep_.threshold = thresholdSpin_->value();
    currentStep_.timeout = timeoutSpin_->value();
    currentStep_.sleepMs = sleepSpin_->value();
    currentStep_.clickOffset = QPoint(xSpin_->value(), ySpin_->value());
    currentStep_.onSuccess = onSuccessEdit_->text().trimmed();
    currentStep_.onFail = onFailEdit_->text().trimmed();
    currentStep_.maxIterations = maxIterSpin_->value();
    currentStep_.loopUntilImage = untilImageEdit_->text().trimmed();
    currentStep_.failReason = failReasonEdit_->text().trimmed();

    emit stepChanged(currentStep_);
}

void StepPropertyPanel::onAddImage() {
    // 由外部处理
}

void StepPropertyPanel::onRemoveImage() {
    int row = imageList_->currentRow();
    if (row >= 0 && row < currentStep_.images.size()) {
        currentStep_.images.removeAt(row);
        updateImageList();
        emit stepChanged(currentStep_);
    }
}

void StepPropertyPanel::onCaptureImage() {
    emit captureRequested();
}

void StepPropertyPanel::onSelectImage() {
    emit selectImageRequested();
}

void StepPropertyPanel::updateUIForType(StepType type) {
    // 根据类型显示/隐藏相关属性组
    bool needImage = (type == StepType::WaitClick || type == StepType::WaitAppear ||
                      type == StepType::WaitDisappear || type == StepType::IfExist ||
                      type == StepType::IfExistClick);
    bool needTime = (type == StepType::WaitClick || type == StepType::WaitAppear ||
                     type == StepType::WaitDisappear || type == StepType::Sleep);
    bool needPos = (type == StepType::ClickPos || type == StepType::WaitClick ||
                    type == StepType::IfExistClick);
    bool needGoto = (type == StepType::WaitClick || type == StepType::WaitAppear ||
                     type == StepType::IfExist || type == StepType::Goto);
    bool needLoop = (type == StepType::Loop || type == StepType::LoopUntil);
    bool needFail = (type == StepType::EndFail);

    imageGroup_->setVisible(needImage);
    timeGroup_->setVisible(needTime);
    posGroup_->setVisible(needPos);
    gotoGroup_->setVisible(needGoto);
    loopGroup_->setVisible(needLoop);
    failGroup_->setVisible(needFail);

    // 更新标签
    if (type == StepType::ClickPos) {
        posGroup_->setTitle(QStringLiteral("点击坐标"));
    } else {
        posGroup_->setTitle(QStringLiteral("点击偏移"));
    }
}

void StepPropertyPanel::updateImageList() {
    imageList_->clear();
    for (const auto& img : currentStep_.images) {
        imageList_->addItem(QFileInfo(img).fileName());
    }
}
