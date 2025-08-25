#include "AutomationPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QSizePolicy>
#include <QFont>
#include <qDebug>

AutomationPanel::AutomationPanel(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("运行指令"));
    resize(520, 420);
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setSpacing(8);

    root->addWidget(makeAutoBattleGroup());
    root->addWidget(makeDailyTasksGroup());
    root->addWidget(makePresetGroup());
    root->addWidget(makeSpecialGroup());

    // 底部留白
    root->addStretch(1);
}

/* ---------------- 分组：自动战斗 ---------------- */
QWidget* AutomationPanel::makeAutoBattleGroup() {
    QGroupBox *box = new QGroupBox(tr("自动战斗"), this);
    QGridLayout *grid = new QGridLayout(box);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    grid->setContentsMargins(8,8,8,8);

    QStringList names{
        tr("国家争霸"), tr("世界争霸"), tr("剿灭将领"), tr("军备合成"),
        tr("国家战争"), tr("征战")
    };

    QList<QPushButton*> btns;
    int colCount = 4; // 每行最多 4 个，自动换行
    for (int i=0; i<names.size(); ++i) {
        QPushButton *b = makeBtn(names[i], SLOT(onTaskClicked()));
        btns << b;
        grid->addWidget(b, i/colCount, i%colCount);
    }
    unifyButtonSizes(btns);
    return box;
}

/* ---------------- 分组：日常任务 ---------------- */
QWidget* AutomationPanel::makeDailyTasksGroup() {
    QGroupBox *box = new QGroupBox(tr("日常任务"), this);
    QGridLayout *grid = new QGridLayout(box);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);
    grid->setContentsMargins(8,8,8,8);

    // 按你图上的顺序尽量还原
    QStringList names{
        tr("英雄中心"), tr("战争学院"), tr("国家宝箱"), tr("将领抽奖"),
        tr("参谋抽奖"), tr("火炮抽奖"), tr("配件抽奖"), tr("英雄技能"),
        tr("矿石夺宝"), tr("月卡领取"), tr("充值活动"), tr("每日签到"),
        tr("周任务"),   tr("公共领取"), tr("公会合战")
    };

    QList<QPushButton*> btns;
    int colCount = 4;
    for (int i=0; i<names.size(); ++i) {
        QPushButton *b = makeBtn(names[i], SLOT(onTaskClicked()));
        btns << b;
        grid->addWidget(b, i/colCount, i%colCount);
    }
    unifyButtonSizes(btns);
    return box;
}

/* ---------------- 分组：一键执行（预设） ---------------- */
QWidget* AutomationPanel::makePresetGroup() {
    QGroupBox *box = new QGroupBox(tr("一键执行"), this);
    QHBoxLayout *h = new QHBoxLayout(box);
    h->setSpacing(8);
    h->addStretch();

    QList<QPushButton*> btns;
    for (int i=1; i<=4; ++i) {
        QPushButton *b = new QPushButton(tr("预设%1").arg(i), box);
        b->setObjectName(QStringLiteral("preset_%1").arg(i));
        b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(b, &QPushButton::clicked, this, &AutomationPanel::onPresetClicked);
        btns << b;
        h->addWidget(b);
    }
    h->addStretch();
    unifyButtonSizes(btns);
    return box;
}

/* ---------------- 分组：特色功能 ---------------- */
QWidget* AutomationPanel::makeSpecialGroup() {
    QGroupBox *box = new QGroupBox(tr("特色功能"), this);
    QHBoxLayout *h = new QHBoxLayout(box);
    h->setContentsMargins(8,8,8,8);

    QPushButton *b = new QPushButton(tr("开卡国战"), box);
    b->setObjectName(QStringLiteral("special_开卡国战"));
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont f = b->font(); f.setBold(true); b->setFont(f);

    connect(b, &QPushButton::clicked, this, &AutomationPanel::onSpecialClicked);
    h->addWidget(b);
    return box;
}

/* ---------------- 工具：创建统一样式按钮 ---------------- */
QPushButton* AutomationPanel::makeBtn(const QString &text, const char *slotConn) {
    QPushButton *b = new QPushButton(text, this);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (slotConn)
        connect(b, SIGNAL(clicked()), this, slotConn);
    return b;
}

void AutomationPanel::unifyButtonSizes(const QList<QPushButton*> &btns) {
    // 取最大 sizeHint 宽度，统一设置最小宽度，视觉整齐
    int w = 0, h = 0;
    for (auto *b : btns) { w = qMax(w, b->sizeHint().width()); h = qMax(h, b->sizeHint().height()); }
    for (auto *b : btns)  { b->setMinimumSize(w, h); }
}

/* ---------------- 槽：任务/预设/特色 ---------------- */
void AutomationPanel::onTaskClicked() {
    if (auto *b = qobject_cast<QPushButton*>(sender())) {
        const QString plan = b->text().trimmed();   // 如 “每日签到”
        emit commandChosen(plan);                   // 新：统一出口
        accept();
    }
}

void AutomationPanel::onPresetClicked() {
    if (auto *b = qobject_cast<QPushButton*>(sender())) {
        // 对象名 preset_1..4
        QString name = b->objectName();
        bool ok = false;
        int idx = name.section('_', 1, 1).toInt(&ok);
        if (ok && idx >= 1 && idx <= 4) {
            emit commandChosen(QStringLiteral("预设%1").arg(idx)); // 新：统一出口（与你的 worker 名称对齐）
            accept();
        }
    }
}

void AutomationPanel::onSpecialClicked() {
    if (auto *b = qobject_cast<QPushButton*>(sender())) {
        const QString plan = b->text().trimmed();  // 如 “开卡固战”
        emit commandChosen(plan);                  // 新：统一出口
        accept();
    }
}

