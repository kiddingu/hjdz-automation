#ifndef GAMEAUTOMATION_H
#define GAMEAUTOMATION_H
#pragma once
#include <QWebEngineView>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <QMouseEvent>
#include <QDateTime>
#include <QTextEdit>
#include <QTimer>
#include <QDebug>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>

class GameAutomation {
public:
    // 一次性：在 view 中查找模板并点击
    static bool findAndClick(QWebEngineView* view,
                             const QString& templatePath,
                             double threshold,
                             QTextEdit* log = nullptr,
                             bool multiScale = true);

    // 多次重试：间隔 retryIntervalMs，最多 retryCount 次
    static void findAndClickRetry(QWebEngineView* view,
                                  const QString& templatePath,
                                  double threshold,
                                  int retryCount,
                                  int retryIntervalMs,
                                  QTextEdit* log = nullptr,
                                  bool multiScale = true);

private:
    static cv::Mat qimageToMat(const QImage& img);
    static bool captureView(QWebEngineView* view, QImage& out);
    static bool matchTemplateMulti(const cv::Mat& sceneGray,
                                   const cv::Mat& templGray,
                                   double threshold,
                                   QPoint& matchPt,
                                   double& bestScore,
                                   bool multiScale);
    static void synthClick(QWebEngineView* view, const QPoint& pt);
    static void appendLog(QTextEdit* log, const QString& msg);
};

#endif // GAMEAUTOMATION_H
