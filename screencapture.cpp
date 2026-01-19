#include "screencapture.h"

#include <QWebEngineView>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPixmap>

// ============== ScreenCapture ==============

ScreenCapture::ScreenCapture(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
}

ScreenCapture::~ScreenCapture() {
}

void ScreenCapture::setGameView(QWebEngineView* view) {
    gameView_ = view;
}

void ScreenCapture::startCapture() {
    if (!gameView_) {
        emit cancelled();
        return;
    }

    selecting_ = false;
    captured_ = false;
    selectionRect_ = QRect();

    captureScreenshot();

    // 设置窗口大小和位置与游戏视图一致
    QPoint globalPos = gameView_->mapToGlobal(QPoint(0, 0));
    setGeometry(globalPos.x(), globalPos.y(), gameView_->width(), gameView_->height());

    show();
    raise();
    activateWindow();
    setFocus();
}

void ScreenCapture::cancelCapture() {
    hide();
    emit cancelled();
}

void ScreenCapture::captureScreenshot() {
    if (!gameView_) return;

    // 截取游戏视图的当前画面
    QPixmap pixmap = gameView_->grab();
    screenshot_ = pixmap.toImage();
}

void ScreenCapture::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setFocus();
}

void ScreenCapture::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);

    // 绘制截图背景
    if (!screenshot_.isNull()) {
        painter.drawImage(0, 0, screenshot_);
    }

    // 绘制半透明遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 100));

    // 如果正在选择或已选择，绘制选择框
    QRect selectRect = selectionRect_;
    if (selecting_) {
        selectRect = QRect(startPos_, currentPos_).normalized();
    }

    if (!selectRect.isEmpty()) {
        // 在选择区域内显示原始图像（去掉遮罩）
        if (!screenshot_.isNull()) {
            painter.drawImage(selectRect, screenshot_, selectRect);
        }

        // 绘制选择框边框
        painter.setPen(QPen(QColor(0, 174, 255), 2));
        painter.drawRect(selectRect);

        // 绘制尺寸信息
        QString sizeText = QString("%1 x %2").arg(selectRect.width()).arg(selectRect.height());
        QRect textRect = selectRect;
        textRect.setTop(selectRect.bottom() + 5);
        textRect.setHeight(20);

        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter, sizeText);
    }

    // 绘制提示文字
    painter.setPen(Qt::white);
    painter.drawText(10, 25, QStringLiteral("框选要截取的区域，按 Enter 确认，按 Esc 取消"));
}

void ScreenCapture::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        selecting_ = true;
        startPos_ = event->pos();
        currentPos_ = event->pos();
        selectionRect_ = QRect();
        update();
    }
}

void ScreenCapture::mouseMoveEvent(QMouseEvent* event) {
    if (selecting_) {
        currentPos_ = event->pos();
        update();
    }
}

void ScreenCapture::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && selecting_) {
        selecting_ = false;
        selectionRect_ = QRect(startPos_, currentPos_).normalized();

        // 确保选择区域至少有一定大小
        if (selectionRect_.width() < 5 || selectionRect_.height() < 5) {
            selectionRect_ = QRect();
        }

        update();
    }
}

void ScreenCapture::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        cancelCapture();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finishCapture();
    }
}

void ScreenCapture::finishCapture() {
    if (selectionRect_.isEmpty() || screenshot_.isNull()) {
        cancelCapture();
        return;
    }

    // 截取选择区域的图片
    QImage capturedImage = screenshot_.copy(selectionRect_);

    hide();
    emit captured(capturedImage, selectionRect_);
}

// ============== ScreenCaptureDialog ==============

ScreenCaptureDialog::ScreenCaptureDialog(const QImage& image, QWidget* parent)
    : QWidget(parent, Qt::Dialog), image_(image)
{
    setWindowTitle(QStringLiteral("保存截图"));
    setFixedSize(400, 300);

    QVBoxLayout* layout = new QVBoxLayout(this);

    // 预览图片
    previewLabel_ = new QLabel(this);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumHeight(150);
    previewLabel_->setStyleSheet("QLabel { background-color: #333; border: 1px solid #555; }");

    // 缩放图片以适应预览区域
    QPixmap pixmap = QPixmap::fromImage(image);
    if (pixmap.width() > 350 || pixmap.height() > 140) {
        pixmap = pixmap.scaled(350, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    previewLabel_->setPixmap(pixmap);
    layout->addWidget(previewLabel_);

    // 显示尺寸信息
    QLabel* sizeLabel = new QLabel(
        QStringLiteral("图片尺寸: %1 x %2 像素").arg(image.width()).arg(image.height()),
        this);
    sizeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(sizeLabel);

    // 文件名输入
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(QStringLiteral("文件名:"), this));
    fileNameEdit_ = new QLineEdit(this);
    fileNameEdit_->setPlaceholderText(QStringLiteral("输入图片名称(不含扩展名)"));
    nameLayout->addWidget(fileNameEdit_);
    nameLayout->addWidget(new QLabel(QStringLiteral(".png"), this));
    layout->addLayout(nameLayout);

    // 按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton* saveBtn = new QPushButton(QStringLiteral("保存"), this);
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, this, &ScreenCaptureDialog::onSave);
    btnLayout->addWidget(saveBtn);

    QPushButton* cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &ScreenCaptureDialog::onCancel);
    btnLayout->addWidget(cancelBtn);

    layout->addLayout(btnLayout);

    fileNameEdit_->setFocus();
}

QString ScreenCaptureDialog::getFileName() const {
    return fileNameEdit_->text().trimmed();
}

void ScreenCaptureDialog::onSave() {
    QString fileName = getFileName();
    if (fileName.isEmpty()) {
        fileNameEdit_->setFocus();
        return;
    }

    emit accepted(fileName, image_);
    close();
}

void ScreenCaptureDialog::onCancel() {
    emit rejected();
    close();
}
