#include "GameAutomation.h"

cv::Mat GameAutomation::qimageToMat(const QImage& img) {
    QImage conv = img.convertToFormat(QImage::Format_RGBA8888);
    cv::Mat mat(conv.height(), conv.width(), CV_8UC4,
                const_cast<uchar*>(conv.bits()), conv.bytesPerLine());
    cv::Mat matBGR; // OpenCV 习惯用 BGR
    cv::cvtColor(mat, matBGR, cv::COLOR_RGBA2BGR);
    return matBGR;
}

bool GameAutomation::captureView(QWebEngineView* view, QImage& out) {
    if (!view || !view->windowHandle()) return false;

    // 抓取窗口内容（物理像素，考虑高 DPI）
    WId wid = view->windowHandle()->winId();
    QScreen* screen = view->windowHandle()->screen();
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return false;

    // 只截取该控件区域（相对窗口的几何），避免把整个窗口都截了
    // 先整窗截图，再裁剪成控件区域：
    QPixmap full = screen->grabWindow(wid);
    if (full.isNull()) return false;

    QPoint topLeft = view->mapTo(view->window(), QPoint(0,0));
    QRect r(topLeft, view->size());
    QPixmap cropped = full.copy(r.intersected(full.rect()));

    out = cropped.toImage();
    return !out.isNull();
}

bool GameAutomation::matchTemplateMulti(const cv::Mat& sceneGray,
                                        const cv::Mat& templGray,
                                        double threshold,
                                        QPoint& matchPt,
                                        double& bestScore,
                                        bool multiScale)
{
    bestScore = -1.0;
    QPoint best;

    std::vector<double> scales;
    if (multiScale) {
        // 根据经验可调，覆盖 0.8~1.2 倍
        for (double s = 0.8; s <= 1.2; s += 0.05) scales.push_back(s);
    } else {
        scales.push_back(1.0);
    }

    for (double s : scales) {
        cv::Mat templScaled;
        if (s != 1.0) {
            cv::resize(templGray, templScaled, cv::Size(), s, s, cv::INTER_AREA);
            if (templScaled.cols > sceneGray.cols || templScaled.rows > sceneGray.rows) continue;
        } else {
            templScaled = templGray;
        }

        cv::Mat result;
        int rw = sceneGray.cols - templScaled.cols + 1;
        int rh = sceneGray.rows - templScaled.rows + 1;
        if (rw <= 0 || rh <= 0) continue;

        result.create(rh, rw, CV_32FC1);
        cv::matchTemplate(sceneGray, templScaled, result, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

        if (maxVal > bestScore) {
            bestScore = maxVal;
            best = QPoint(maxLoc.x + templScaled.cols/2, maxLoc.y + templScaled.rows/2);
        }
    }

    if (bestScore >= threshold) {
        matchPt = best;
        return true;
    }
    return false;
}

void GameAutomation::synthClick(QWebEngineView* view, const QPoint& pt) {
    if (!view) return;

    // 构造并投递鼠标事件（相对 view 客户区坐标）
    QPoint local = pt;
    QPoint global = view->mapToGlobal(local);

    QMouseEvent press(QEvent::MouseButtonPress, local, global,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, local, global,
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

    // 注意：对 WebEngine 来说，把事件发给 focusProxy 往往更稳
    QWidget* target = view->focusProxy() ? view->focusProxy() : view;

    QCoreApplication::sendEvent(target, &press);
    QCoreApplication::sendEvent(target, &release);
}

void GameAutomation::appendLog(QTextEdit* log, const QString& msg) {
    if (!log) return;
    log->append("[" + QDateTime::currentDateTime().toString("hh:mm:ss") + "] " + msg);
}

bool GameAutomation::findAndClick(QWebEngineView* view,
                                  const QString& templatePath,
                                  double threshold,
                                  QTextEdit* log,
                                  bool multiScale)
{
    if (!view) { appendLog(log, "❌ view == nullptr"); return false; }

    QImage shot;
    if (!captureView(view, shot)) {
        appendLog(log, "❌ 截图失败（可能窗口被遮挡或最小化）");
        return false;
    }

    cv::Mat scene = qimageToMat(shot);
    cv::Mat templ = cv::imread(templatePath.toStdString(), cv::IMREAD_COLOR);
    if (templ.empty()) { appendLog(log, "❌ 模板读取失败: " + templatePath); return false; }

    cv::Mat sceneGray, templGray;
    cv::cvtColor(scene, sceneGray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(templ, templGray, cv::COLOR_BGR2GRAY);

    // 轻度去噪 & 直方图均衡
    cv::GaussianBlur(sceneGray, sceneGray, cv::Size(3,3), 0);
    cv::equalizeHist(sceneGray, sceneGray);

    QPoint hit;
    double score = -1.0;
    bool ok = matchTemplateMulti(sceneGray, templGray, threshold, hit, score, multiScale);

    if (!ok) {
        appendLog(log, QString("🔎 未找到模板（阈值=%1，best=%2）").arg(threshold).arg(score, 0, 'f', 3));
        return false;
    }

    appendLog(log, QString("✅ 命中模板：score=%1，点击 (%2,%3)")
                       .arg(score, 0, 'f', 3).arg(hit.x()).arg(hit.y()));

    synthClick(view, hit);
    return true;
}

void GameAutomation::findAndClickRetry(QWebEngineView* view,
                                       const QString& templatePath,
                                       double threshold,
                                       int retryCount,
                                       int retryIntervalMs,
                                       QTextEdit* log,
                                       bool multiScale)
{
    if (retryCount <= 0) retryCount = 1;
    auto attempt = new int(0);

    auto timer = new QTimer(view);
    timer->setInterval(retryIntervalMs);

    QObject::connect(timer, &QTimer::timeout, view, [=]() {
        (*attempt)++;
        appendLog(log, QString("⏱ 重试 %1/%2...").arg(*attempt).arg(retryCount));

        bool ok = findAndClick(view, templatePath, threshold, log, multiScale);
        if (ok || *attempt >= retryCount) {
            if (!ok) appendLog(log, "❌ 重试结束：仍未命中模板");
            timer->stop();
            timer->deleteLater();
            delete attempt;
        }
    });

    // 立即来一发，然后进入定时重试
    bool ok = findAndClick(view, templatePath, threshold, log, multiScale);
    if (ok) {
        delete attempt;
        return;
    }
    timer->start();
}
