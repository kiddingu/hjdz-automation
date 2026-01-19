#ifndef SCREENCAPTURE_H
#define SCREENCAPTURE_H

#include <QWidget>
#include <QImage>
#include <QRect>
#include <QPoint>

class QWebEngineView;

// 截图工具
// 允许用户在游戏画面上框选区域截取模板图片
class ScreenCapture : public QWidget {
    Q_OBJECT
public:
    explicit ScreenCapture(QWidget* parent = nullptr);
    ~ScreenCapture();

    // 设置要截图的游戏视图
    void setGameView(QWebEngineView* view);

    // 开始截图模式（显示半透明覆盖层）
    void startCapture();

    // 取消截图
    void cancelCapture();

signals:
    // 截图完成，返回截取的图片和区域
    void captured(const QImage& image, const QRect& region);
    // 截图取消
    void cancelled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void captureScreenshot();
    void finishCapture();

    QWebEngineView* gameView_ = nullptr;
    QImage screenshot_;         // 截取的完整游戏画面
    QPoint startPos_;           // 框选起始点
    QPoint currentPos_;         // 当前鼠标位置
    QRect selectionRect_;       // 选择的区域
    bool selecting_ = false;    // 是否正在框选
    bool captured_ = false;     // 是否已完成截图
};

// 截图保存对话框
class ScreenCaptureDialog : public QWidget {
    Q_OBJECT
public:
    explicit ScreenCaptureDialog(const QImage& image, QWidget* parent = nullptr);

    // 获取用户输入的文件名
    QString getFileName() const;

    // 获取截取的图片
    QImage getImage() const { return image_; }

signals:
    void accepted(const QString& fileName, const QImage& image);
    void rejected();

private slots:
    void onSave();
    void onCancel();

private:
    QImage image_;
    class QLineEdit* fileNameEdit_;
    class QLabel* previewLabel_;
};

#endif // SCREENCAPTURE_H
