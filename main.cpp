#include <QApplication>
#include <QWebEngineSettings>
#include <QDir>
#include <QFile>
#include <QDebug>
#include "mainwindow.h"
#include "taskmodel.h"

int main(int argc, char *argv[])
{
    // 注册自定义类型，用于信号槽跨线程传递
    qRegisterMetaType<TaskDefinition>("TaskDefinition");
    // 0) 关掉 Chromium/QtWebEngine 控制台刷屏（先设置环境变量）
    QByteArray quietFlags = "--disable-logging --log-level=3";
    qputenv("QT_LOGGING_RULES", "qt.webengine.*=false");

    // 1) 组合 PPAPI Flash 参数
    //    注意：pepflashplayer.dll 必须是与你应用同位数（64/32）的 PPAPI 版本
    const QString flashPath = QDir::toNativeSeparators(QDir::currentPath() + "/pepflashplayer.dll");
    QByteArray flashFlags;
    if (QFile::exists(flashPath)) {
        // 根据你的 DLL 版本号填写（示例：34.0.0.301）
        QString flags = QString(
                            "--ppapi-flash-path=\"%1\" "
                            "--ppapi-flash-version=34.0.0.301 "
                            // 下面几个开关常用于旧版 Flash 的兼容（可按需裁剪）
                            "--allow-outdated-plugins "
                            "--enable-plugins "
                            "--allow-running-insecure-content "
                            ).arg(flashPath);
        flashFlags = flags.toUtf8();
    } else {
        qWarning() << "pepflashplayer.dll not found at" << flashPath;
    }

    // 2)（可选）某些环境下 Flash 需要禁用 sandbox
    //    如果你遇到“插件无法加载/崩溃”，可以打开下面这一行再试：
    // quietFlags += " --no-sandbox";

    // 3) 合并并设置 QTWEBENGINE_CHROMIUM_FLAGS（一定要在 QApplication 之前）
    QByteArray allFlags = quietFlags;
    if (!flashFlags.isEmpty()) {
        allFlags += " ";
        allFlags += flashFlags;
    }
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", allFlags);

    // 4) 高分屏
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // 5) 启动 Qt 应用
    QApplication app(argc, argv);

    // 6) 启用插件等设置
    auto s = QWebEngineSettings::defaultSettings();
    s->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    s->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    s->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    s->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);

    MainWindow w;
    w.show();
    return app.exec();
}
