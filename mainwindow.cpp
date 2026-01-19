#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QTextEdit>
#include <QWebEngineView>
#include <QMessageBox>
#include <qlabel.h>
#include <QComboBox>
#include <QSpinBox>
#include <QTimer>
#include <QCheckBox>
#include <QListWidget>
#include <QPixmap>
#include <QImage>
#include <QMenuBar>
#include <opencv2/opencv.hpp>
#include "mywebpage.h"
#include "AutomationPanel.h"
#include "AutomationWorker.h"
#include "SilentWebPage.h"
#include "taskeditor.h"
#include "taskmodel.h"
#include <QStandardPaths>
#include <QDir>
#include <QWebEngineSettings>
#include <QCoreApplication>
#include <QBuffer>
#include <QByteArray>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *central = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // 顶部按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *openGameBtn = new QPushButton(QStringLiteral("打开游戏"));
    QPushButton *qqAccountBtn = new QPushButton(QStringLiteral("QQ账号"));
    QPushButton *raAccountBtn = new QPushButton(QStringLiteral("红警账号"));
    // 新增：给三个“全部”按钮起名字，便于 connect
    QPushButton *btnRunAll     = new QPushButton(QStringLiteral("全部执行"));
    QPushButton *btnStopAll    = new QPushButton(QStringLiteral("全部停止"));
    QPushButton *btnRefreshAll = new QPushButton(QStringLiteral("全部刷新"));
    QPushButton *btnOther      = new QPushButton(QStringLiteral("其它功能"));
    QPushButton *btnClearLog   = new QPushButton(QStringLiteral("清空日志"));

    buttonLayout->addWidget(openGameBtn);
    buttonLayout->addWidget(qqAccountBtn);
    buttonLayout->addWidget(raAccountBtn);
    buttonLayout->addWidget(btnRunAll);
    buttonLayout->addWidget(btnStopAll);
    buttonLayout->addWidget(btnRefreshAll);
    buttonLayout->addWidget(btnOther);
    buttonLayout->addWidget(btnClearLog);
    mainLayout->addLayout(buttonLayout);

    // 现有连接
    connect(openGameBtn, &QPushButton::clicked, this, &MainWindow::openGameDialog);
    connect(qqAccountBtn, &QPushButton::clicked, this, &MainWindow::openQQDialog);
    connect(raAccountBtn, &QPushButton::clicked, this, &MainWindow::openRedAlertDialog);

    // 新增“全部”按钮连接
    connect(btnRunAll,     &QPushButton::clicked, this, &MainWindow::onRunAll);
    connect(btnStopAll,    &QPushButton::clicked, this, &MainWindow::onStopAll);
    connect(btnRefreshAll, &QPushButton::clicked, this, &MainWindow::onRefreshAll);

    // 清空右侧全局日志（顺便可清空所有标签页日志，按需）
    connect(btnClearLog, &QPushButton::clicked, this, [=](){
        if (globalLogOutput) globalLogOutput->clear();
    });
    // 下方左右布局
    QHBoxLayout *bottomLayout = new QHBoxLayout;

    // 左侧账号操作日志（多个标签页）
    logTabWidget = new QTabWidget;
    logTabWidget->setTabsClosable(false);

    connect(logTabWidget, &QTabWidget::tabCloseRequested, this, [=](int index) {
        qDebug()<<"logTabWidget tabClose requested";
        QWidget *widget = logTabWidget->widget(index);
        logTabWidget->removeTab(index);
        widget->deleteLater();
    });

    // 右侧所有账号日志
    globalLogOutput = new QTextEdit;
    globalLogOutput->setReadOnly(true);

    bottomLayout->addWidget(logTabWidget, 3);
    bottomLayout->addWidget(globalLogOutput, 2);

    mainLayout->addLayout(bottomLayout);

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("红警多开"));

}

MainWindow::~MainWindow() {}

void MainWindow::onRefreshAll()
{
    for (auto &ctx : gameWindows_) {
        if (!ctx.view) continue;
        if (ctx.log) ctx.log->append(QStringLiteral("[全部操作] 刷新游戏"));
        ctx.view->reload();
    }
}
void MainWindow::onRunAll()
{
    if (gameWindows_.isEmpty()) {
        if (globalLogOutput)
            globalLogOutput->append(QStringLiteral("[提示] 当前没有打开的游戏窗口，无法批量执行。"));
        return;
    }

    auto *dlg = new AutomationPanel(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setModal(false);

    connect(dlg, &AutomationPanel::commandChosen,
            this, &MainWindow::onExecuteAll,
            Qt::QueuedConnection);

    dlg->open();
}
void MainWindow::onExecuteAll(const QString& planName)
{
    const QString plan = planName.trimmed();
    if (plan.isEmpty()) {
        if (globalLogOutput)
            globalLogOutput->append(QStringLiteral("[提示] 未选择任何命令，已取消批量执行。"));
        return;
    }

    // 处理特殊命令：打开任务编辑器
    if (plan == QStringLiteral("__TASK_EDITOR__")) {
        openTaskEditor();
        return;
    }

    int launched = 0;
    for (auto &ctx : gameWindows_) {
        if (!ctx.view || !ctx.log) continue;

        // 如果该窗口已有任务在跑（ctx.active），先请求停
        if (ctx.active) {
            ctx.log->append(QStringLiteral("[全部执行] 检测到正在运行的任务，先停止。"));
            stopAutomationFor(ctx.view, ctx.log);
        }

        ctx.log->append(QStringLiteral("[全部执行] 开始执行：%1").arg(plan));
        startAutomationFor(ctx.view, ctx.log, plan);
        ++launched;
    }

    if (globalLogOutput) {
        globalLogOutput->append(
            QStringLiteral("[全部执行] 已向 %1 个窗口下发命令：%2").arg(launched).arg(plan));
    }
}
void MainWindow::onStopAll()
{
    if (gameWindows_.isEmpty()) {
        if (globalLogOutput)
            globalLogOutput->append(QStringLiteral("[提示] 当前没有打开的游戏窗口。"));
        return;
    }

    for (auto &ctx : gameWindows_) {
        if (!ctx.view) continue;
        if (ctx.log) ctx.log->append(QStringLiteral("[全部操作] 请求停止"));
        stopAutomationFor(ctx.view, ctx.log);
    }
}
GameWindowCtx* MainWindow::ctxFor(QWebEngineView* v) {
    if (!v) return nullptr;
    auto it = gameWindows_.find(v);
    return (it == gameWindows_.end()) ? nullptr : &it.value();
}
GameWindowCtx& MainWindow::ensureCtx(QWebEngineView* v) {
    auto it = gameWindows_.find(v);
    if (it == gameWindows_.end()) {
        GameWindowCtx ctx;
        ctx.view = v;
        it = gameWindows_.insert(v, std::move(ctx));
    }
    return it.value();
}
void MainWindow::ensureThreadAndWorker(GameWindowCtx& ctx) {
    if (!ctx.stop) ctx.stop = QSharedPointer<StopToken>::create();

    if (!ctx.thread) {
        ctx.thread = new QThread(this);
        ctx.thread->setObjectName(QString("WorkerThread-%1")
                                      .arg(reinterpret_cast<quintptr>(ctx.view.data()), 0, 16));
    }
    if (!ctx.worker) {
        ctx.worker = new AutomationWorker(ctx.view, ctx.stop, nullptr); // 父设 nullptr 才能 moveToThread
        ctx.worker->moveToThread(ctx.thread);

        // 日志转发
        connect(ctx.worker, &AutomationWorker::log, this, [wlog = QPointer<QTextEdit>(ctx.log)](const QString& s){
            if (wlog) wlog->append(QString("[%1] %2")
                                 .arg(QTime::currentTime().toString("HH:mm:ss"), s));
        });

        connect(ctx.worker, &AutomationWorker::logThumb, this,
                [log = QPointer<QTextEdit>(ctx.log)](const QString& action, const QStringList& paths, double thr, int ms, double scale){
                    if (!log || paths.isEmpty()) return;

                    const QString timestamp = QTime::currentTime().toString("HH:mm:ss");
                    QString html = QString("<div>[%1] [步骤] %2：").arg(timestamp, action);

                    for (int i = 0; i < paths.size(); ++i) {
                        const QString& path = paths[i];
                        const QString base = QFileInfo(path).fileName();

                        QImage img;
                        if (img.load(path)) {
                            // --- 核心修改点：从 addResource 改为 Base64 编码 ---
                            const int maxW = 48;
                            int w = img.width(), h = img.height();
                            if (w > maxW) {
                                h = int(h * (double(maxW) / w));
                                w = maxW;
                            }
                            QImage thumb = img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);

                            // 1. 将缩略图保存到内存中的 QByteArray
                            QByteArray ba;
                            QBuffer buffer(&ba);
                            buffer.open(QIODevice::WriteOnly);
                            thumb.save(&buffer, "PNG"); // 指定格式为 PNG

                            // 2. 将 QByteArray 转换为 Base64 编码的字符串
                            QString base64_data = QString::fromLatin1(ba.toBase64());

                            // 3. 构建一个自包含的 <img> 标签
                            html.append(QString(" %1 <img src=\"data:image/png;base64,%2\" style=\"vertical-align:middle;margin-left:2px;\">")
                                            .arg(base, base64_data));

                        } else {
                            html.append(" " + base + "[加载失败]");
                        }

                        if (i < paths.size() - 1) {
                            html.append(" |");
                        }
                    }

                    html.append(QString(" (阈值=%1, 超时=%2ms)</div>").arg(thr,0,'f',2).arg(ms));

                    log->append(html);
                });

        connect(ctx.worker, &AutomationWorker::finished, this, [wlog = QPointer<QTextEdit>(ctx.log)](const QString& plan){
            if (wlog) wlog->append(QStringLiteral("%1 执行完毕").arg(plan));
        });
        connect(ctx.worker, &AutomationWorker::aborted, this, [wlog = QPointer<QTextEdit>(ctx.log)](const QString& reason){
            if (wlog) wlog->append(QStringLiteral("[中断] %1").arg(reason));
        });

        // 任务结束 → 线程退出；线程退出 → 释放 worker；更新 ctx 状态
        connect(ctx.worker, &AutomationWorker::finished, ctx.thread, &QThread::quit);
        connect(ctx.worker, &AutomationWorker::aborted,  ctx.thread, &QThread::quit);

        connect(ctx.thread, &QThread::finished, ctx.worker, &QObject::deleteLater);
        connect(ctx.thread, &QThread::finished, this, [this, pView = QPointer<QWebEngineView>(ctx.view)](){
            // 线程结束时，清理对应 ctx 的线程/worker指针与状态
            if (!pView) return;
            if (auto c = ctxFor(pView)) {
                c->worker = nullptr;
                c->thread = nullptr;
                c->active = false;
                c->lastActive = QDateTime::currentDateTime();
                if (c->log) c->log->append(QStringLiteral("[自动化] 已停止并清理。"));
            }
        });
    }

    if (!ctx.thread->isRunning()) ctx.thread->start();
}
void MainWindow::teardownThreadAndWorker(GameWindowCtx& ctx) {
    if (ctx.thread) {
        // 不 terminate；quit 等自然退出
        ctx.thread->quit();
        ctx.thread->wait(1500);
        ctx.thread->deleteLater();
        ctx.thread = nullptr;
    }
    // worker 会在 finished→deleteLater，这里仅置空
    ctx.worker = nullptr;
}
void MainWindow::removeTabIfAny(GameWindowCtx& ctx) {
    if (!ctx.tab) return;
    // 假设你有成员 logTabWidget（未展示）；如果是局部的，改成合适的容器操作。
    // 这里用防御式删除：直接 deleteLater 即可（若在 QTabWidget 内，请先 removeTab）
    QWidget* w = ctx.tab;
    ctx.tab = nullptr;
    if (w) w->deleteLater();
}
void MainWindow::registerGameWindow(QWebEngineView* view, QTextEdit* log, QWidget* tab) {
    if (!view) return;
    auto& ctx = ensureCtx(view);
    ctx.view = view;
    ctx.log  = log;
    ctx.tab  = tab;
    if (!ctx.stop) ctx.stop = QSharedPointer<StopToken>::create();

    // 窗口销毁 → 统一反注册
    connect(view, &QObject::destroyed, this, [this, wv = QPointer<QWebEngineView>(view)](){
        if (wv) unregisterGameWindow(wv);
    });
}
void MainWindow::unregisterGameWindow(QWebEngineView* view) {
    auto ctx = ctxFor(view);
    if (!ctx) return;

    // 1) 请求停止任务
    if (ctx->stop) ctx->stop->cancelled.store(true, std::memory_order_relaxed);

    // 2) 回收线程/worker
    teardownThreadAndWorker(*ctx);

    // 3) 移除 Tab（若你是 QTabWidget，需要先 removeTab，这里简化为直接 deleteLater）
    removeTabIfAny(*ctx);

    // 4) 移出容器
    gameWindows_.remove(view);
}
void MainWindow::startAutomationFor(QWebEngineView *view, QTextEdit *log, const QString &planName)
{
    if (!view || planName.isEmpty()) return;

    auto ctx = ctxFor(view);
    if (!ctx) {
        // 允许外部直接 start（未 register 的情况）→ 创建一个轻量 ctx
        registerGameWindow(view, log, /*tab*/nullptr);
        ctx = ctxFor(view);
    }
    if (ctx->active) {
        if (ctx->log) ctx->log->append(QStringLiteral("[提示] 该窗口已有任务在运行，请先停止后再执行。"));
        return;
    }

    // 刷新 log 引用（以最新为准）
    ctx->log = log;

    // 清零 token，建/启 子线程 & worker
    if (!ctx->stop) ctx->stop = QSharedPointer<StopToken>::create();
    ctx->stop->cancelled.store(false, std::memory_order_relaxed);

    ensureThreadAndWorker(*ctx);
    ctx->active = true;
    ctx->lastActive = QDateTime::currentDateTime();

    // 在 worker 所属线程里启动任务
    QMetaObject::invokeMethod(ctx->worker, "runTask",
                              Qt::QueuedConnection,
                              Q_ARG(QString, planName));
}
void MainWindow::stopAutomationFor(QWebEngineView *view, QTextEdit *log)
{
    auto ctx = ctxFor(view);
    if (!ctx || !ctx->thread || !ctx->worker) {
        if (log) log->append(QStringLiteral("[提示] 当前没有正在运行的任务。"));
        return;
    }
    if (ctx->stop) ctx->stop->cancelled.store(true, std::memory_order_relaxed);
    ctx->thread->quit();
    if (log) log->append(QStringLiteral("[自动化] 已请求停止"));
}

void MainWindow::openGameDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("选择要打开的账号"));
    dialog.resize(300, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QListWidget *listWidget = new QListWidget;
    layout->addWidget(listWidget);

    // 存储：每一项对应的数据
    struct RedAccountEntry {
        QString qq;
        QString link;
        int region;
    };
    QVector<RedAccountEntry> redAccounts;

    QFile file("qq_accounts.json");
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const QJsonValue &val : arr) {
                if (!val.isObject()) continue;
                QJsonObject obj = val.toObject();
                QString qq = obj["qq"].toString();
                QString link = obj["link"].toString();

                if (!obj.contains("red") || !obj["red"].isArray()) continue;
                QJsonArray reds = obj["red"].toArray();

                for (const QJsonValue &r : reds) {
                    if (!r.isObject()) continue;
                    QJsonObject red = r.toObject();

                    int region = red["region"].toInt();
                    QString remark = red["remark"].toString();
                    if (region <= 0 || link.isEmpty()) continue;

                    QString display = QStringLiteral("%1 %2 %3区").arg(qq, remark, QString::number(region));

                    QListWidgetItem *item = new QListWidgetItem;
                    QWidget *widget = new QWidget;
                    QHBoxLayout *hbox = new QHBoxLayout(widget);
                    QCheckBox *check = new QCheckBox(display);
                    hbox->addWidget(check);
                    hbox->addStretch();
                    hbox->setContentsMargins(0, 0, 0, 0);
                    widget->setLayout(hbox);

                    listWidget->addItem(item);
                    listWidget->setItemWidget(item, widget);
                    item->setSizeHint(widget->sizeHint());

                    redAccounts.append({qq, link, region});
                }
            }
        }
    }

    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, [&]() {
        for (int i = 0; i < listWidget->count(); ++i) {
            QListWidgetItem *item = listWidget->item(i);
            QWidget *widget = listWidget->itemWidget(item);
            if (!widget) continue;

            QCheckBox *check = widget->findChild<QCheckBox *>();
            if (!check || !check->isChecked()) continue;

            const RedAccountEntry &entry = redAccounts[i];

            QUrl url(entry.link);
            QUrlQuery query(url);
            QString openid = query.queryItemValue("openid");
            QString openkey = query.queryItemValue("openkey");
            QString pfkey = query.queryItemValue("pfkey");

            if (openid.isEmpty() || openkey.isEmpty() || pfkey.isEmpty()) continue;

            int region = entry.region - 1; // 注意减1
            QString finalUrl = QString("https://tankstorm-qqgame.sincetimes.com/"
                                       "?openid=%1&openkey=%2&pf=qqgame&pfkey=%3&region=%4&qz_ver=6&appcanvas=1&via=&abc=%5")
                                   .arg(openid, openkey, pfkey)
                                   .arg(region)
                                   .arg(QString::number(QDateTime::currentMSecsSinceEpoch()));

            // 打开 Flash 游戏链接（用 Qt 内嵌浏览器）
            QWebEngineView *gameView = new QWebEngineView;
            gameView->setAttribute(Qt::WA_DeleteOnClose, true);

            QWebEngineProfile *pf = profileForQQ(entry.qq);
            auto *page = new SilentWebPage(pf, gameView);
            gameView->setPage(page);

            // ✅ 关键：页面级也开启（有时 profile 设置被后续代码覆盖，页面再开一遍更稳）
            auto *s = page->settings();
            s->setAttribute(QWebEngineSettings::PluginsEnabled, true);
            s->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
            s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
            s->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
            s->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);

            gameView->setWindowTitle(QStringLiteral("游戏窗口 - %1").arg(entry.qq));
            gameView->resize(960, 800);
            gameView->load(QUrl(finalUrl));


            // === 日志页：容器 + 日志框 + 按钮栏 ===
            QWidget *tabContainer = new QWidget;
            QVBoxLayout *tabVBox = new QVBoxLayout(tabContainer);
            tabVBox->setContentsMargins(6, 6, 6, 6);
            tabVBox->setSpacing(6);

            // 日志框
            QTextEdit *gameLog = new QTextEdit;
            gameLog->setReadOnly(true);
            tabVBox->addWidget(gameLog, /*stretch*/1);

            // 按钮行
            QHBoxLayout *btnRow = new QHBoxLayout;
            btnRow->setContentsMargins(0, 0, 0, 0);

            registerGameWindow(gameView, gameLog, tabContainer);

            QPushButton *btnReload = new QPushButton(QStringLiteral("刷新游戏"));
            QPushButton *btnExec   = new QPushButton(QStringLiteral("执行命令"));
            QPushButton *btnStop   = new QPushButton(QStringLiteral("停止命令"));
            QPushButton *btnClear  = new QPushButton(QStringLiteral("清空输出"));

            btnRow->addWidget(btnReload);
            btnRow->addWidget(btnExec);
            btnRow->addWidget(btnStop);
            btnRow->addWidget(btnClear);
            btnRow->addStretch();

            tabVBox->addLayout(btnRow);

            // 添加标签页（标题例如：QQ (区号)）
            QString tabTitle = QStringLiteral("%1 (%2区)").arg(entry.qq).arg(entry.region);
            int tabIndex = logTabWidget->addTab(tabContainer, tabTitle);
            logTabWidget->setCurrentIndex(tabIndex);

            // 绑定按钮行为
            connect(btnReload, &QPushButton::clicked, this, [gameView, gameLog]() {
                gameLog->append(QStringLiteral("[操作] 刷新游戏..."));
                gameView->reload();
            });

            connect(btnExec, &QPushButton::clicked, this, [=]() {
                auto *dlg = new AutomationPanel(this);
                dlg->setAttribute(Qt::WA_DeleteOnClose, true);
                connect(dlg, &AutomationPanel::commandChosen, this, [=](const QString& plan){
                    // 处理特殊命令：打开任务编辑器（只针对当前窗口）
                    if (plan == QStringLiteral("__TASK_EDITOR__")) {
                        openTaskEditor(gameView);  // 传入当前窗口
                        return;
                    }
                    if (gameLog) gameLog->append(QStringLiteral("[执行] %1").arg(plan));
                    startAutomationFor(gameView, gameLog, plan);
                });
                dlg->open();
            });

            // 停止按钮：
            connect(btnStop, &QPushButton::clicked, this, [=]() {
                if (gameLog) gameLog->append(QStringLiteral("[操作] 停止命令。"));
                stopAutomationFor(gameView, gameLog);
            });

            connect(btnClear, &QPushButton::clicked, this, [=]() {
                gameLog->clear();
            });

            // 窗口关闭时，自动移除对应日志页
            connect(gameView, &QObject::destroyed, this, [=]() {
                int idx = logTabWidget->indexOf(tabContainer);
                if (idx != -1) {
                    QWidget *w = logTabWidget->widget(idx);
                    logTabWidget->removeTab(idx);
                    qDebug()<<"tab removed";
                    w->deleteLater();
                }
            });


            // 显示游戏窗口
            gameView->show();




        }

        dialog.accept();
    });

    connect(btnBox, &QDialogButtonBox::rejected, [&]() {
        dialog.reject();
    });

    dialog.exec();
}
void MainWindow::openRedAlertDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("红警账号管理"));
    dialog.resize(800, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QTableWidget *raTable = new QTableWidget(0, 5, &dialog);
    raTable->setHorizontalHeaderLabels({
        QStringLiteral("行号"), QStringLiteral("备注"), QStringLiteral("QQ号"), QStringLiteral("区服"), QStringLiteral("操作")
    });
    raTable->horizontalHeader()->setStretchLastSection(true);
    raTable->verticalHeader()->setVisible(false);

    layout->addWidget(raTable);

    QPushButton *addBtn = new QPushButton(QStringLiteral("添加红警账号"));
    QPushButton *closeBtn = new QPushButton(QStringLiteral("关闭窗口"));
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(addBtn);
    buttonLayout->addWidget(closeBtn);
    layout->addLayout(buttonLayout);

    QFile file("qq_accounts.json");
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const QJsonValue &val : arr) {
                if (!val.isObject()) continue;
                QJsonObject obj = val.toObject();
                QString qq = obj["qq"].toString();

                if (obj.contains("red") && obj["red"].isArray()) {
                    QJsonArray redArray = obj["red"].toArray();
                    for (const QJsonValue &rval : redArray) {
                        if (!rval.isObject()) continue;
                        QJsonObject red = rval.toObject();

                        // 不再检查 region == 0，而是只展示真正有数据的项
                        int region = red["region"].toInt();
                        QString remark = red["remark"].toString();

                        // 添加到表格
                        int row = raTable->rowCount();
                        raTable->insertRow(row);
                        raTable->setItem(row, 0, new QTableWidgetItem(QString::number(red["order"].toInt())));
                        raTable->item(row, 0)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                        raTable->setItem(row, 1, new QTableWidgetItem(remark));
                        raTable->setItem(row, 2, new QTableWidgetItem(qq));
                        raTable->setItem(row, 3, new QTableWidgetItem(QString::number(region)));
                        setupRAOperationButtons(raTable, row);
                    }
                }

            }
        }
    }

    // 添加按钮事件
    connect(addBtn, &QPushButton::clicked, [&]() {
        QDialog inputDialog(&dialog);
        inputDialog.setWindowTitle(QStringLiteral("添加红警账号"));
        QVBoxLayout *vbox = new QVBoxLayout(&inputDialog);

        // 备注输入
        QHBoxLayout *remarkLayout = new QHBoxLayout;
        QLabel *remarkLabel = new QLabel(QStringLiteral("备注："));
        QLineEdit *remarkEdit = new QLineEdit;
        remarkLayout->addWidget(remarkLabel);
        remarkLayout->addWidget(remarkEdit);
        vbox->addLayout(remarkLayout);

        // QQ号下拉框（从QQ管理中提取）
        QHBoxLayout *qqLayout = new QHBoxLayout;
        QLabel *qqLabel = new QLabel(QStringLiteral("QQ账号："));
        QComboBox *qqCombo = new QComboBox;

        QFile file("qq_accounts.json");
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isArray()) {
                for (const QJsonValue &val : doc.array()) {
                    if (val.isObject()) {
                        QJsonObject obj = val.toObject();
                        qqCombo->addItem(obj["qq"].toString());
                    }
                }
            }
        }
        qqLayout->addWidget(qqLabel);
        qqLayout->addWidget(qqCombo);
        vbox->addLayout(qqLayout);

        // 区服输入（限制范围 1-100）
        QHBoxLayout *regionLayout = new QHBoxLayout;
        QLabel *regionLabel = new QLabel(QStringLiteral("区服："));
        QSpinBox *regionSpin = new QSpinBox;
        regionSpin->setRange(1, 100);
        regionLayout->addWidget(regionLabel);
        regionLayout->addWidget(regionSpin);
        vbox->addLayout(regionLayout);

        // OK / Cancel 按钮
        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        vbox->addWidget(buttonBox);

        QObject::connect(buttonBox, &QDialogButtonBox::accepted, [&]() {
            int row = raTable->rowCount();
            raTable->insertRow(row);
            raTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));  // 行号
            raTable->item(row, 0)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            raTable->setItem(row, 1, new QTableWidgetItem(remarkEdit->text()));
            raTable->setItem(row, 2, new QTableWidgetItem(qqCombo->currentText()));
            raTable->setItem(row, 3, new QTableWidgetItem(QString::number(regionSpin->value())));

            setupRAOperationButtons(raTable, row);  // 添加操作按钮
            inputDialog.accept();

            // 保存
            saveRedAccountsToFile(raTable);
        });

        QObject::connect(buttonBox, &QDialogButtonBox::rejected, [&]() {
            inputDialog.reject();
        });

        inputDialog.exec();
    });


    connect(closeBtn, &QPushButton::clicked, [&]() {
        dialog.accept();
    });

    dialog.exec();
}
void MainWindow::setupRAOperationButtons(QTableWidget *table, int row) {
    QWidget *widget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    // 设置按钮
    QPushButton *setBtn = new QPushButton(QStringLiteral("设置"));
    connect(setBtn, &QPushButton::clicked, this, [=]() {
        QString qq = table->item(row, 1)->text();  // QQ号
        QString region = table->item(row, 3)->text(); // 区服

        QMessageBox::information(this, QStringLiteral("设置操作"),
                                 QStringLiteral("你可以在这里打开一个设置界面\nQQ: %1\n区服: %2").arg(qq, region));

        // TODO：在这里打开你定义的设置窗口或功能
    });

    // 删除按钮
    QPushButton *delBtn = new QPushButton(QStringLiteral("删除"));
    connect(delBtn, &QPushButton::clicked, this, [=]() {
        QString targetQQ = table->item(row, 2)->text();
        int targetRegion = table->item(row, 3)->text().toInt();

        // 更新 JSON 文件，移除对应 QQ 的该 red 区服
        QFile file("qq_accounts.json");
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();

            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (int i = 0; i < arr.size(); ++i) {
                    QJsonObject obj = arr[i].toObject();
                    if (obj["qq"].toString() == targetQQ && obj.contains("red") && obj["red"].isArray()) {
                        QJsonArray redArray = obj["red"].toArray();
                        QJsonArray newRedArray;
                        for (const QJsonValue &redVal : redArray) {
                            QJsonObject redObj = redVal.toObject();
                            if (redObj["region"].toInt() != targetRegion) {
                                newRedArray.append(redObj);  // 保留其他区服
                            }
                        }
                        obj["red"] = newRedArray;
                        arr[i] = obj;
                        break;
                    }
                }

                // 写回 JSON 文件
                QFile outFile("qq_accounts.json");
                if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    outFile.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
                    outFile.close();
                }
            }
        }

        // 从表格中删除该行
        table->removeRow(row);
    });

    layout->addWidget(setBtn);
    layout->addWidget(delBtn);
    layout->addStretch();
    widget->setLayout(layout);

    table->setCellWidget(row, 4, widget); // 操作列通常在最后一列
}
void MainWindow::saveRedAccountsToFile(QTableWidget *raTable) {
    QFile file("qq_accounts.json");
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;

    QJsonArray accountArray = doc.array();

    // 先清空所有账号的 red 数组
    for (QJsonValueRef accVal : accountArray) {
        if (accVal.isObject()) {
            QJsonObject acc = accVal.toObject();
            acc["red"] = QJsonArray();
            accVal = acc;
        }
    }

    // 按照 raTable 的数据构建 red 项，并插入到对应 qq 的 red 数组中
    for (int row = 0; row < raTable->rowCount(); ++row) {
        QString qq = raTable->item(row, 2)->text();
        QJsonObject redEntry;
        redEntry["order"] = row + 1;
        redEntry["remark"] = raTable->item(row, 1)->text();
        redEntry["region"] = raTable->item(row, 3)->text().toInt();
        redEntry["setting"] = "";

        // 在对应 qq 中插入 redEntry
        for (int i = 0; i < accountArray.size(); ++i) {
            QJsonObject acc = accountArray[i].toObject();
            if (acc["qq"].toString() == qq) {
                QJsonArray redList = acc["red"].toArray();
                redList.append(redEntry);
                acc["red"] = redList;
                accountArray[i] = acc;
                break;
            }
        }
    }

    QFile outFile("qq_accounts.json");
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        outFile.write(QJsonDocument(accountArray).toJson(QJsonDocument::Indented));
        outFile.close();
    }
}
void MainWindow::openQQDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("QQ账号管理"));
    dialog.resize(800, 400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QTableWidget *qqTable = new QTableWidget(0, 7, &dialog);
    qqTable->setHorizontalHeaderLabels({
        QStringLiteral("备注"), QStringLiteral("QQ账号"), QStringLiteral("密码"),
        QStringLiteral("游戏ID"), QStringLiteral("链接"), QStringLiteral("状态"), QStringLiteral("操作")
    });
    qqTable->horizontalHeader()->setStretchLastSection(true);

    QFile file("qq_accounts.json");
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (const QJsonValue &val : arr) {
                    if (!val.isObject()) continue;
                    QJsonObject obj = val.toObject();

                    QString qq = obj["qq"].toString();
                    QString password = obj["password"].toString();
                    QString id = obj["id"].toString();
                    QString link = obj["link"].toString();
                    QString remark = obj["remark"].toString();

                    int row = qqTable->rowCount();
                    qqTable->insertRow(row);
                    qqTable->setItem(row, 0, new QTableWidgetItem(remark));
                    qqTable->setItem(row, 1, new QTableWidgetItem(qq));
                    qqTable->setItem(row, 2, new QTableWidgetItem(password));
                    qqTable->setItem(row, 3, new QTableWidgetItem(id));
                    qqTable->setItem(row, 4, new QTableWidgetItem(link));
                    QTableWidgetItem *statusItem = new QTableWidgetItem(QStringLiteral("待测试"));
                    statusItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled); // 不可编辑
                    qqTable->setItem(row, 5, statusItem);
                    setupOperationButtons(qqTable, row);
                }
            }
        }
    } else {
        file.open(QIODevice::WriteOnly);
        file.write("[]");
        file.close();
    }

    layout->addWidget(qqTable);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *addBtn = new QPushButton(QStringLiteral("添加QQ账号"));
    QPushButton *closeBtn = new QPushButton(QStringLiteral("关闭窗口"));
    buttonLayout->addStretch();
    buttonLayout->addWidget(addBtn);
    buttonLayout->addWidget(closeBtn);
    layout->addLayout(buttonLayout);

    connect(addBtn, &QPushButton::clicked, [&]() {
        int row = qqTable->rowCount();
        qqTable->insertRow(row);
        for (int i = 0; i < 5; ++i)
            qqTable->setItem(row, i, new QTableWidgetItem(""));

        setStatus(qqTable, row, QStringLiteral("待测试"));
        setupOperationButtons(qqTable, row);
    });

    connect(&dialog, &QDialog::finished, [&]() {
        saveQQAccountsToFile(qqTable);
    });

    connect(closeBtn, &QPushButton::clicked, [&]() {
        dialog.accept();
    });

    dialog.exec();
}
void MainWindow::setupOperationButtons(QTableWidget *table, int row) {
    QWidget *opWidget = new QWidget;
    QHBoxLayout *opLayout = new QHBoxLayout(opWidget);
    QPushButton *loginBtn = new QPushButton(QStringLiteral("登录"));
    QPushButton *testBtn = new QPushButton(QStringLiteral("测试"));
    QPushButton *delBtn = new QPushButton(QStringLiteral("删除"));
    opLayout->addWidget(loginBtn);
    opLayout->addWidget(testBtn);
    opLayout->addWidget(delBtn);
    opLayout->setContentsMargins(0, 0, 0, 0);
    opWidget->setLayout(opLayout);
    table->setCellWidget(row, 6, opWidget);

    connect(delBtn, &QPushButton::clicked, this, [=]() {
        table->removeRow(row);
        saveQQAccountsToFile(table);
    });


    connect(loginBtn, &QPushButton::clicked, this, [=]() {
        const QString loginUrl = QStringLiteral("http://qqgame.qq.com/webappframe/?appid=10407");

        // ✅ 从当前行读取 QQ 号（QQ 管理表：列1是“QQ账号”）
        const QString qq = (table->item(row, 1) ? table->item(row, 1)->text() : QString());
        if (qq.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先填写该行的 QQ 账号再登录。"));
            return;
        }

        // ✅ 为该 QQ 获取/创建持久化 profile（免登录、与其他QQ隔离）
        QWebEngineProfile *pf = profileForQQ(qq);

        QWebEngineView *accountWebView = new QWebEngineView;
        auto *page = new MyWebPage(pf, accountWebView); // 用 profile 构造 Page（你的 MyWebPage 已支持）
        accountWebView->setPage(page);

        QDialog *loginDialog = new QDialog(this);
        loginDialog->setWindowTitle(QStringLiteral("QQ登录 - %1").arg(qq));
        loginDialog->resize(600, 500);
        auto *vbox = new QVBoxLayout(loginDialog);
        vbox->addWidget(accountWebView);

        // 加载登录页
        accountWebView->load(QUrl(loginUrl));

        // 控制台捕获 token/openid，写回该行“链接”列
        connect(page, &MyWebPage::tokenCapturedFromConsole, this, [=](const QString &message) {
            QUrl url(message.trimmed());
            QUrlQuery query(url);
            const QString token  = query.queryItemValue("access_token");
            const QString openid = query.queryItemValue("openid");

            if (!token.isEmpty() && !openid.isEmpty()) {
                const QString fullUrl = url.toString();
                table->setItem(row, 4, new QTableWidgetItem(fullUrl)); // 列4=链接
                setStatus(table, row, QStringLiteral("有效"));
                saveQQAccountsToFile(table);
                loginDialog->accept();
            }
        });

        connect(loginDialog, &QDialog::finished, this, [=]() {
            accountWebView->deleteLater();
            loginDialog->deleteLater();
        });

        loginDialog->exec();  // 模态显示，保证能完成登录流程
    });





    connect(testBtn, &QPushButton::clicked, this, [=]() {
        QString link = table->item(row, 4)->text();
        if (link.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("该账号没有可用链接！"));
            return;
        }

        QWebEngineView *testView = new QWebEngineView;  // 不加入到界面中
        MyWebPage *testPage = new MyWebPage(testView);
        testView->setPage(testPage);

        QTimer *timeoutTimer = new QTimer(this);
        timeoutTimer->setSingleShot(true);
        bool *resultCaptured = new bool(false);  // 标记是否已处理

        setStatus(table, row, QStringLiteral("测试中"));

        connect(testPage, &MyWebPage::consoleMessageCaptured, this, [=](const QString &message) {
            if (*resultCaptured) return;

            if (message.contains("key-value", Qt::CaseInsensitive)) {
                setStatus(table, row, QStringLiteral("已过期"));
                *resultCaptured = true;
                timeoutTimer->stop();
            } else if (message.contains("initializing", Qt::CaseInsensitive)) {
                setStatus(table, row, QStringLiteral("有效"));
                *resultCaptured = true;
                timeoutTimer->stop();
            }
        });

        connect(timeoutTimer, &QTimer::timeout, this, [=]() {
            if (!*resultCaptured) {
                setStatus(table, row, QStringLiteral("超时"));
            }

            // 清理资源
            testView->deleteLater();
            timeoutTimer->deleteLater();
            delete resultCaptured;
        });

        timeoutTimer->start(20000);  // 20 秒超时

        testView->load(QUrl(link));  // 后台加载，不显示任何界面
    });


}
void MainWindow::saveQQAccountsToFile(QTableWidget *qqTable) {
    QJsonArray arr;

    // 读取旧数据用于保留 red 字段
    QMap<QString, QJsonArray> previousRedMap;  // QQ号 -> red数组
    QFile oldFile("qq_accounts.json");
    if (oldFile.open(QIODevice::ReadOnly)) {
        QJsonDocument oldDoc = QJsonDocument::fromJson(oldFile.readAll());
        oldFile.close();
        if (oldDoc.isArray()) {
            for (const QJsonValue &val : oldDoc.array()) {
                if (val.isObject()) {
                    QJsonObject obj = val.toObject();
                    QString qq = obj["qq"].toString();
                    if (obj.contains("red") && obj["red"].isArray()) {
                        previousRedMap[qq] = obj["red"].toArray();
                    }
                }
            }
        }
    }

    for (int row = 0; row < qqTable->rowCount(); ++row) {
        QJsonObject obj;
        obj["order"] = row;
        obj["qq"] = qqTable->item(row, 1)->text();
        obj["password"] = qqTable->item(row, 2)->text();
        obj["id"] = qqTable->item(row, 3)->text();
        obj["link"] = qqTable->item(row, 4)->text();
        obj["remark"] = qqTable->item(row, 0)->text();

        QString qq = obj["qq"].toString();
        // 保留该 QQ 对应的 red 数组（若有）
        QJsonArray redArray = previousRedMap.value(qq, QJsonArray());
        obj["red"] = redArray;

        arr.append(obj);
    }

    QJsonDocument doc(arr);
    QFile outFile("qq_accounts.json");
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        outFile.write(doc.toJson(QJsonDocument::Indented));
        outFile.close();
    } else {
        qWarning() << "无法保存到 qq_accounts.json";
    }
}
void MainWindow::setStatus(QTableWidget *table, int row, const QString &statusText) {
    QTableWidgetItem *item = new QTableWidgetItem(statusText);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable); // 设置为不可编辑
    table->setItem(row, 5, item);
}
void MainWindow::emitLogToGameTab(QTextEdit *tab, const QString &text) {
    if (tab) {
        tab->append("[" + QDateTime::currentDateTime().toString("hh:mm:ss") + "] " + text);
    }
}QWebEngineProfile* MainWindow::profileForQQ(const QString& qq)
{
    if (profiles_.contains(qq) && profiles_[qq])
        return profiles_[qq];

    // 使用当前程序目录/web_profile/<qq>
    QString base = QCoreApplication::applicationDirPath() + "/web_profile/" + qq;

    // 创建缓存和存储目录
    QDir().mkpath(base + "/cache");
    QDir().mkpath(base + "/storage");

    qDebug() << "[ProfileForQQ] 使用缓存目录:" << base;

    // 每个 QQ 独立 profile
    auto *profile = new QWebEngineProfile(QStringLiteral("qq_%1").arg(qq), this);
    profile->setCachePath(base + "/cache");
    profile->setPersistentStoragePath(base + "/storage");
    profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

    // 启用常用特性（Flash、JS、跨域、本地内容访问等）
    auto *ps = profile->settings();
    ps->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    ps->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    ps->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    ps->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    ps->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);

    profiles_.insert(qq, profile);
    return profile;
}

// 打开任务编辑器
// targetView: 指定目标窗口，nullptr 表示针对全部窗口
void MainWindow::openTaskEditor(QWebEngineView* targetView)
{
    if (!taskEditor_) {
        taskEditor_ = new TaskEditor(this);
        connect(taskEditor_, &TaskEditor::runTaskRequested,
                this, &MainWindow::onRunScriptTask);
    }

    // 保存目标窗口 (nullptr = 全部窗口)
    taskEditorTargetView_ = targetView;

    // 设置游戏视图用于截图
    if (targetView) {
        // 如果有指定目标窗口，使用它
        taskEditor_->setGameView(targetView);
    } else if (!gameWindows_.isEmpty()) {
        // 否则使用第一个窗口
        auto it = gameWindows_.begin();
        if (it->view) {
            taskEditor_->setGameView(it->view);
        }
    }

    taskEditor_->show();
    taskEditor_->raise();
    taskEditor_->activateWindow();

    QString targetDesc = targetView ? QStringLiteral("当前窗口") : QStringLiteral("全部窗口");
    if (globalLogOutput) {
        globalLogOutput->append(QStringLiteral("[任务编辑器] 已打开可视化任务编辑器 (目标: %1)").arg(targetDesc));
    }
}

// 执行脚本任务
void MainWindow::onRunScriptTask(const TaskDefinition& task)
{
    if (gameWindows_.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("没有可用的游戏窗口"));
        return;
    }

    // 检查是否有指定的目标窗口
    QWebEngineView* targetView = taskEditorTargetView_.data();

    int launched = 0;
    for (auto &ctx : gameWindows_) {
        if (!ctx.view || !ctx.log) continue;

        // 如果指定了目标窗口，只执行该窗口
        if (targetView && ctx.view != targetView) {
            continue;  // 跳过非目标窗口
        }

        // 如果该窗口已有任务在跑，先请求停止
        if (ctx.active) {
            ctx.log->append(QStringLiteral("[脚本任务] 检测到正在运行的任务，先停止。"));
            stopAutomationFor(ctx.view, ctx.log);
        }

        // 重置停止令牌（重要！stopAutomationFor 会设置 cancelled=true）
        if (!ctx.stop) ctx.stop = QSharedPointer<StopToken>::create();
        ctx.stop->cancelled.store(false, std::memory_order_relaxed);

        // 确保线程和worker已创建
        ensureThreadAndWorker(ctx);

        ctx.log->append(QStringLiteral("[脚本任务] 开始执行：%1").arg(task.name));
        ctx.active = true;
        ctx.lastActive = QDateTime::currentDateTime();

        // 通过信号调用 worker 的 runScriptTask
        QMetaObject::invokeMethod(ctx.worker, "runScriptTask",
                                  Qt::QueuedConnection,
                                  Q_ARG(TaskDefinition, task));
        ++launched;
    }

    QString targetDesc = targetView ? QStringLiteral("指定窗口") : QStringLiteral("全部窗口");
    if (globalLogOutput) {
        globalLogOutput->append(
            QStringLiteral("[脚本任务] 已向 %1 (%2个) 下发任务：%3")
                .arg(targetDesc).arg(launched).arg(task.name));
    }
}
