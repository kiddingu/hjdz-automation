#include "AutomationWorker.h"
#include "StopToken.h"

#include <QWebEngineView>
#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QPixmap>
#include <QBuffer>
#include <QWindow>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <QRegularExpression>
#include "imgdsl_qt.h"
#include <memory>


class AWToolbox : public imgdsl::IToolbox {
public:
    explicit AWToolbox(AutomationWorker* w) : w_(w) {}

    imgdsl::MatchResult findImage(const QString& path, double th,
                                  const QRect& /*roi*/, bool /*multiScale*/) override {
        imgdsl::MatchResult mr; mr.which = path;
        if (!w_) return mr;
        QImage img = w_->capture();
        if (img.isNull()) return mr;
        double sc = 0.0;
        QPoint pt = w_->findTemplatePlaceholder(img, path, &sc, th);
        if (pt.x() >= 0) { mr.matched = true; mr.point = pt; mr.score = sc; }
        return mr;
    }

    bool clickLogical(const QPoint& logicalPt) override {
        return w_ ? w_->clickAt(logicalPt) : false;
    }

    void sleepMs(int ms) override { w_ ? w_->sleepMs(ms) : QThread::msleep(static_cast<unsigned long>(ms)); }

    void logAction(const QString& action, const QString& conditionName, int timeout, const imgdsl::MatchResult* result) override {
        if (!w_) return;

        QString actionText = action;
        if (action.toUpper() == "WAIT_UNTIL") actionText = QStringLiteral("寻找");
        else if (action.toUpper() == "CLICK") actionText = QStringLiteral("点击");

        QStringList pathsToLog;

        if (result && result->matched) {
            pathsToLog << result->which;
        }
        else {
            QRegularExpression re("APPEAR\\(([^\\)]+)\\)");
            auto it = re.globalMatch(conditionName);
            while (it.hasNext()) {
                pathsToLog << it.next().captured(1).trimmed();
            }
        }

        if (pathsToLog.isEmpty()) {
            QString msg = QString("[%1] %2").arg(action, conditionName);
            if (timeout > 0) msg.append(QStringLiteral(" (超时=%1ms)").arg(timeout));
            emit w_->log(msg);
            return;
        }

        emit w_->logThumb(actionText, pathsToLog, 0.85, timeout, 1.0);
    }

    void logInfo(const QString& message) override {
        if (w_) emit w_->log(QString("[信息] %1").arg(message));
    }

    void logError(const QString& message) override {
        if (w_) emit w_->log(QString("[错误] %1").arg(message));
    }

    void logSuccess(const QString& message) override {
        if (w_) emit w_->log(QString("[成功] %1").arg(message));
    }

    // 【修正】补全缺失的接口实现
    void setTaskContext(const QString& taskName) override {
        currentTaskName_ = taskName;
    }
    void clearTaskContext() override {
        currentTaskName_.clear();
    }
    QString resolveImagePath(const QString& imageNameOrPath) const override {

        if (imageNameOrPath.contains('/')) {
            return imageNameOrPath;
        }
        if (!currentTaskName_.isEmpty()) {
            QString fullPath = QString("游戏图片/%1/%2.png").arg(currentTaskName_, imageNameOrPath);
            return fullPath;
        }
        qWarning() << "[AWToolbox] No task context set for simple image name:" << imageNameOrPath;
        return imageNameOrPath;
    }

private:
    AutomationWorker* w_{};
    QString currentTaskName_; // 【修正】添加成员变量
};
AutomationWorker::~AutomationWorker()
{
    if (imgdsl::toolbox() == toolbox_.get()) {
        // ...那么在我被销毁之前，必须将全局指针清空
        imgdsl::set_toolbox(nullptr);
        qDebug() << "AutomationWorker destroyed, global toolbox has been cleared.";
    } else {
        qDebug() << "AutomationWorker destroyed, but it was not the active toolbox.";
    }
}
AutomationWorker::AutomationWorker(QWebEngineView* view,
                                   QSharedPointer<StopToken> stop,
                                   QObject* parent)
    : QObject(parent), view_(view), stop_(std::move(stop))
{
    toolbox_ = std::make_unique<AWToolbox>(this);
}
// ====== 2) 读图工具：支持资源路径和中文文件路径 ======
static cv::Mat imreadSafe(const QString& filePath, int flags = cv::IMREAD_COLOR) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[imreadWithChinesePath] Cannot open file:" << filePath;
        return {};
    }
    QByteArray data = file.readAll();
    file.close();
    std::vector<uchar> buf(data.begin(), data.end());
    return cv::imdecode(buf, flags);
}
// ====== 3) 截图：优先 QWidget::grab()（逻辑像素），不可见时回退 QScreen::grabWindow()（设备像素） ======
QImage AutomationWorker::capture() {
    QImage img;
    QMetaObject::invokeMethod(view_, [this, &img]() {
        // 任选其一：grab() 是逻辑像素；grabWindow(winId()) 是设备像素
        QPixmap px = view_->grab();   // 推荐：逻辑像素，避免 DPR 换算
        img = px.toImage();
    }, Qt::BlockingQueuedConnection);
    return img;
}
// ====== 4) 模板匹配：返回 view 的“局部逻辑坐标” ======
QPoint AutomationWorker::findTemplatePlaceholder(const QImage& screen,
                                                 const QString& tplPath,
                                                 double* outScore,
                                                 double threshold)
{
    if (outScore) *outScore = 0.0;
    if (screen.isNull()) return QPoint(-1, -1);

    // QImage -> cv::Mat (BGRA → BGR)
    cv::Mat srcRGBA(screen.height(), screen.width(), CV_8UC4,
                    const_cast<uchar*>(screen.bits()), screen.bytesPerLine());
    cv::Mat srcBGR;
    cv::cvtColor(srcRGBA, srcBGR, cv::COLOR_BGRA2BGR);

    // 模板
    cv::Mat tpl = imreadSafe(tplPath, cv::IMREAD_COLOR);
    if (tpl.empty()) {
        qWarning() << "[findTemplatePlaceholder] template empty:" << tplPath;
        return QPoint(-1, -1);
    }

    // 尺寸检查
    int rw = srcBGR.cols - tpl.cols + 1;
    int rh = srcBGR.rows - tpl.rows + 1;
    if (rw <= 0 || rh <= 0) return QPoint(-1, -1);

    // 匹配
    cv::Mat result(rh, rw, CV_32FC1);
    cv::matchTemplate(srcBGR, tpl, result, cv::TM_CCOEFF_NORMED);

    double minVal = 0.0, maxVal = 0.0;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

    if (outScore) *outScore = maxVal;
    if (maxVal < threshold) return QPoint(-1, -1);

    // 命中中心（当前坐标系与 screen 一致）
    int cx = maxLoc.x + tpl.cols / 2;
    int cy = maxLoc.y + tpl.rows / 2;

    const qreal dpr = view_->devicePixelRatioF(); // 例如 1.0、1.25、1.5、2.0 等
    QPoint localLogical( int(cx / dpr), int(cy / dpr) );
    return localLogical;
}
bool AutomationWorker::shouldStop(const char* where) const
{
    if (!stop_) return false;
    if (stop_->cancelled.load(std::memory_order_relaxed)) {
        return true;
    }
    return false;
}

void AutomationWorker::sleepMs(int ms)
{
    // 子线程里安全 sleep；期间让出调度
    QThread::msleep(static_cast<unsigned long>(ms));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

bool AutomationWorker::clickAt(const QPoint& localPos)
{
    if (shouldStop("clickAt/pre")) return false;

    bool ok = false;
    QPointer<QWebEngineView> v = view_;
    if (!v) return false;

    auto isGui = (QThread::currentThread() == v->thread());
    auto type  = isGui ? Qt::DirectConnection : Qt::BlockingQueuedConnection;

    QMetaObject::invokeMethod(v, [v, localPos, &ok]() {
        if (!v) return;

        v->setFocus();
        v->activateWindow();

        QWidget* target = v->focusProxy();
        if (!target) target = v;

        QPoint vpPos = (target == v)
                           ? localPos
                           : target->mapFromGlobal(v->mapToGlobal(localPos));
        QPoint globalPos = target->mapToGlobal(vpPos);

        QMouseEvent press (QEvent::MouseButtonPress,  vpPos, globalPos,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, vpPos, globalPos,
                            Qt::LeftButton, Qt::NoButton,  Qt::NoModifier);

        QCoreApplication::sendEvent(target, &press);
        QCoreApplication::sendEvent(target, &release);
        ok = true;
    }, type);

    if (shouldStop("clickAt/post")) return false;
    return ok;
}

// ===== 回到主界面（救援动作，占位实现） =====

bool AutomationWorker::returnToHome(int maxMs)
{
    emit log(QStringLiteral("[救援] 尝试回到主界面…"));
    // 这里放你的真实“关闭面板/点返回/点主页”等逻辑
    // 简单占位：等待一会儿，当作成功
    int waited = 0;
    while (waited < maxMs) {
        if (shouldStop("returnToHome")) return false;
        sleepMs(200);
        waited += 200;
    }
    emit log(QStringLiteral("[救援] 已尝试回到主界面"));
    return true;
}

// ===== 截图保存 =====

QString AutomationWorker::saveScreenshot(const QString& dir, const QString& tag)
{
    QImage img = capture();
    if (img.isNull()) return QString();

    QDir d(dir);
    if (!d.exists()) {
        if (!d.mkpath(".")) return QString();
    }
    const QString ts  = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    const QString fn  = QString("%1-%2.png").arg(ts, tag);
    const QString abs = d.absoluteFilePath(fn);
    if (img.save(abs)) return abs;
    return QString();
}

// ===== 任务入口 =====

void AutomationWorker::runTask(const QString& planName)
{
    imgdsl::set_toolbox(toolbox_.get()); // 设置全局工具箱
    bool success = false; // 用于记录任务执行结果
    toolbox_->setTaskContext(planName);


    // --- 任务调度器 ---
    if (planName == QStringLiteral("国家争霸")) {
        success = runTask_NationalContest();
    } else if (planName == QStringLiteral("世界争霸")) {
        success = runTask_WorldContest();
    } else if (planName == QStringLiteral("剿灭将领")) {
        success = runTask_AnnihilateGeneral();
    } else if (planName == QStringLiteral("军备合成")) {
        success = runTask_ArmamentSynthesis();
    } else if (planName == QStringLiteral("国家战争")) {
        success = runTask_NationalWar();
    } else if (planName == QStringLiteral("征战")) {
        success = runTask_Campaign();
    } else if (planName == QStringLiteral("英雄中心")) {
        success = runTask_HeroCenter();
    } else if (planName == QStringLiteral("战争学院")) {
        success = runTask_WarAcademy();
    } else if (planName == QStringLiteral("国家宝箱")) {
        success = runTask_NationalTreasure();
    } else if (planName == QStringLiteral("将领抽奖")) {
        success = runTask_GeneralLottery();
    } else if (planName == QStringLiteral("参谋抽奖")) {
        success = runTask_AdvisorLottery();
    } else if (planName == QStringLiteral("火炮抽奖")) {
        success = runTask_ArtilleryLottery();
    } else if (planName == QStringLiteral("配件抽奖")) {
        success = runTask_AccessoryLottery();
    } else if (planName == QStringLiteral("英雄技能")) {
        success = runTask_HeroSkill();
    } else if (planName == QStringLiteral("矿石夺宝")) {
        success = runTask_OrePlunder();
    } else if (planName == QStringLiteral("月卡领取")) {
        success = runTask_MonthlyCard();
    } else if (planName == QStringLiteral("充值活动")) {
        success = runTask_RechargeEvent();
    } else if (planName == QStringLiteral("每日签到")) {
        success = runTask_DailySignIn();
    } else if (planName == QStringLiteral("周任务")) {
        success = runTask_WeeklyTasks();
    } else if (planName == QStringLiteral("公共领取")) {
        success = runTask_PublicCollection();
    } else if (planName == QStringLiteral("公会合战")) {
        success = runTask_GuildBattle();
    } else if (planName == QStringLiteral("开卡国战")) {
        success = runTask_CardNationalWar();
    } else {
        toolbox_->logError(QString("未知的任务计划: %1").arg(planName));
        success = false;
    }

    // 【关键】根据任务执行结果，发出正确的信号
    if (success) {
        emit finished(planName);
    } else {
        // 您可以提供更详细的失败原因
        emit aborted(QString("任务执行失败: %1").arg(planName));
    }
}
// --- 任务函数占位符实现 ---

#define UNIMPLEMENTED_TASK(taskName) \
bool AutomationWorker::taskName() { \
        toolbox_->logError(QString("任务 [%1] 尚未实现。").arg(QString(#taskName).remove("runTask_"))); \
        return false; \
}



bool AutomationWorker::runTask_DailySignIn()
{
    toolbox_->logInfo("开始执行“每日签到”任务...");
    using namespace imgdsl;


    // 1) 打开签到面板
    auto openBtn = IMG("每日签到");
    if (!WAIT_UNTIL(openBtn, 8000)) {
        toolbox_->logError("未找到“每日签到”入口。");
        return false;
    }
    CLICK(openBtn);

    // 2) 点击签到或已签到
    auto signOrDone =
        IMG("签到")||
        IMG("已签到")
        ;
    if (WAIT_UNTIL(signOrDone, 8000)) {
        CLICK(signOrDone);
    } else {
        toolbox_->logInfo("未找到“签到”或“已签到”按钮，继续检查后续奖励...");
    }

    // 3) 检查并点击可选的周奖励
    toolbox_->logInfo("检查周度奖励...");
    if (auto d7 = IMG("7天"))  { CLICK(d7); SLEEP(300); }
    if (auto d14 = IMG("14天")) { CLICK(d14); SLEEP(300); }
    if (auto d21 = IMG("21天")) { CLICK(d21); SLEEP(300); }
    if (auto d28 = IMG("28天")) { CLICK(d28); SLEEP(300); }

    // 4) 关闭窗口
    auto closeBtn = IMG("关闭窗口");
    if (!WAIT_UNTIL(closeBtn, 5000)) {
        toolbox_->logError("未找到关闭按钮，任务可能未完全关闭。");
        return true;
    }
    CLICK(closeBtn);

    toolbox_->logSuccess("“每日签到”任务成功完成。");
    return true;
}

bool AutomationWorker::runTask_NationalContest()
{
    toolbox_->logInfo("开始执行“国家争霸”任务...");
    using namespace imgdsl;

    // 步骤 1: 进入国家争霸界面
    // 假设主界面有一个“国家争霸”的入口按钮
    auto entranceBtn = IMG("国家争霸入口"); // 请替换为实际的入口图片名
    if (!WAIT_UNTIL(entranceBtn, 8000)) {
        toolbox_->logError("未找到“国家争霸”入口。");
        return false;
    }
    CLICK(entranceBtn);

    // 等待进入争霸界面的标志性图片，例如标题
    if (!WAIT_UNTIL(IMG("国家争霸标题"), 5000)) { // 请替换为实际的标题图片名
        toolbox_->logError("进入“国家争霸”界面失败。");
        return false;
    }

    // 步骤 2: 循环挑战，直到出现“购买”按钮提示次数不足
    int maxLoops = 20; // 设置一个最大循环次数，防止无限循环
    for (int i = 0; i < maxLoops; ++i) {
        if (shouldStop("NationalContestLoop")) {
            toolbox_->logError("任务被用户手动停止。");
            return false;
        }

        toolbox_->logInfo(QString("开始第 %1/%2 轮挑战...").arg(i + 1).arg(maxLoops));

        // 定义本轮循环需要处理的各种按钮
        auto challengeBtn = IMG("挑战");      // “挑战”或“征战”按钮
        auto attackBtn = IMG("攻击");        // “攻击”按钮
        auto refreshBtn = IMG("刷新");      // “刷新”按钮
        auto buyBtn = IMG("购买");          // “购买”按钮（次数不足时出现）
        auto closeBuyPanelBtn = IMG("关闭购买"); // 关闭购买弹窗的按钮

        // 创建一个组合条件，代表“可以继续”的任意一种状态
        auto canContinue = ANY(challengeBtn, attackBtn, refreshBtn);

        // 创建一个组合条件，代表“需要处理”的任意一种状态（继续或购买）
        auto anyAction = ANY(canContinue, buyBtn);

        MatchResult result;
        // 等待任意一个可操作的按钮出现
        if (!WAIT_UNTIL(anyAction, 10000, 200, &result)) {
            toolbox_->logError("超时：未找到“挑战”、“攻击”、“刷新”或“购买”按钮。");
            break; // 找不到任何按钮，退出循环
        }

        // 判断是哪个按钮出现了
        // 情况 A: 次数用尽，出现“购买”按钮
        if (result.which == buyBtn.name()) {
            toolbox_->logSuccess("挑战次数已用尽，任务正常结束。");
            // 点击关闭按钮，退出购买弹窗
            if (WAIT_UNTIL(closeBuyPanelBtn, 3000)) {
                CLICK(closeBuyPanelBtn);
            }
            return true; // 任务成功结束
        }

        // 情况 B: 出现了“挑战”、“攻击”或“刷新”
        if (canContinue.last().matched) {
            toolbox_->logInfo(QString("找到可执行操作：%1").arg(QFileInfo(canContinue.last().which).fileName()));
            CLICK(canContinue); // 点击找到的按钮
            SLEEP(1500); // 点击后等待一下，让游戏响应
        }
    }

    toolbox_->logSuccess("“国家争霸”任务已达到最大循环次数。");
    return true;
}
bool AutomationWorker::runTask_WorldContest(){
    return false;
}
bool AutomationWorker::runTask_AnnihilateGeneral(){
    return false;
}
bool AutomationWorker::runTask_ArmamentSynthesis(){
    return false;
}
bool AutomationWorker::runTask_NationalWar(){
    return false;
}
bool AutomationWorker::runTask_Campaign(){
    return false;
}

// 日常任务
bool AutomationWorker::runTask_HeroCenter(){
    return false;
}
bool AutomationWorker::runTask_WarAcademy(){
    return false;
}
bool AutomationWorker::runTask_NationalTreasure(){
    return false;
}
bool AutomationWorker::runTask_GeneralLottery(){
    return false;
}
bool AutomationWorker::runTask_AdvisorLottery(){
    return false;
}
bool AutomationWorker::runTask_ArtilleryLottery(){
    return false;
}
bool AutomationWorker::runTask_AccessoryLottery(){
    return false;
}
bool AutomationWorker::runTask_HeroSkill(){
    return false;
}
bool AutomationWorker::runTask_OrePlunder(){
    return false;
}
bool AutomationWorker::runTask_MonthlyCard(){
    return false;
}
bool AutomationWorker::runTask_RechargeEvent(){
    return false;
}
bool AutomationWorker::runTask_WeeklyTasks(){
    return false;
}
bool AutomationWorker::runTask_PublicCollection(){
    return false;
}
bool AutomationWorker::runTask_GuildBattle(){
    return false;
}

// 特色功能
bool AutomationWorker::runTask_CardNationalWar(){
    return false;
}
