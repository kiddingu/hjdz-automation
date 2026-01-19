#ifndef STEPWIDGET_H
#define STEPWIDGET_H

#include <QWidget>
#include <QFrame>
#include <QGroupBox>
#include "taskmodel.h"

class QLabel;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QListWidget;
class QStackedWidget;

// 步骤列表项组件
class StepListItem : public QFrame {
    Q_OBJECT
public:
    explicit StepListItem(const TaskStep& step, int index, QWidget* parent = nullptr);

    void setStep(const TaskStep& step);
    TaskStep getStep() const { return step_; }

    void setIndex(int index);
    int getIndex() const { return index_; }

    void setSelected(bool selected);
    bool isSelected() const { return selected_; }

signals:
    void clicked(int index);
    void doubleClicked(int index);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateDisplay();
    QString getTypeIcon() const;

    TaskStep step_;
    int index_ = 0;
    bool selected_ = false;

    QLabel* iconLabel_;
    QLabel* nameLabel_;
    QLabel* detailLabel_;
};

// 步骤属性编辑面板
class StepPropertyPanel : public QWidget {
    Q_OBJECT
public:
    explicit StepPropertyPanel(QWidget* parent = nullptr);

    void setStep(const TaskStep& step);
    TaskStep getStep() const;

    void clear();

signals:
    void stepChanged(const TaskStep& step);
    void captureRequested();    // 请求截图
    void selectImageRequested(); // 请求选择图片

private slots:
    void onTypeChanged(int index);
    void onPropertyChanged();
    void onAddImage();
    void onRemoveImage();
    void onCaptureImage();
    void onSelectImage();

private:
    void setupUI();
    void updateUIForType(StepType type);
    void updateImageList();

    TaskStep currentStep_;
    bool updating_ = false;  // 防止循环更新

    // 通用属性
    QComboBox* typeCombo_;
    QLineEdit* idEdit_;
    QLineEdit* descEdit_;

    // 图片相关
    QGroupBox* imageGroup_;
    QListWidget* imageList_;
    QPushButton* addImageBtn_;
    QPushButton* removeImageBtn_;
    QPushButton* captureBtn_;
    QPushButton* selectBtn_;
    QComboBox* matchModeCombo_;
    QDoubleSpinBox* thresholdSpin_;

    // 时间相关
    QGroupBox* timeGroup_;
    QSpinBox* timeoutSpin_;
    QSpinBox* sleepSpin_;

    // 坐标相关
    QGroupBox* posGroup_;
    QSpinBox* xSpin_;
    QSpinBox* ySpin_;

    // 跳转相关
    QGroupBox* gotoGroup_;
    QLineEdit* onSuccessEdit_;
    QLineEdit* onFailEdit_;

    // 循环相关
    QGroupBox* loopGroup_;
    QSpinBox* maxIterSpin_;
    QLineEdit* untilImageEdit_;

    // 失败原因
    QGroupBox* failGroup_;
    QLineEdit* failReasonEdit_;

    // 栈布局用于不同类型的面板
    QStackedWidget* detailStack_;
};

#endif // STEPWIDGET_H
