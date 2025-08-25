#ifndef MYWEBPAGE_H
#define MYWEBPAGE_H

#include <QWebEnginePage>
#include <QUrlQuery>
#include <QDebug>
#include <QFile>
#include <QDateTime>

class MyWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    using QWebEnginePage::QWebEnginePage;

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override {
        Q_UNUSED(level);
        Q_UNUSED(lineNumber);

        // 输出调试信息到控制台
        qDebug() << "\n" << QStringLiteral("message：") << message << "\n"
                 << QStringLiteral("sourceID：") << sourceID << "\n";

        // 写入文件（可选）
        QFile file("console_log.txt");
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out.setCodec("UTF-8");  // 防止中文乱码
            out << "-----------------------------\n";
            out << "message: " << message << "\n";
            out << "sourceID: " << sourceID << "\n";
            out << "line: " << lineNumber << "\n";
            out << "timestamp: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
            file.close();
        }

        // 发出所有控制台消息
        emit consoleMessageCaptured(message);

        // 针对 access_token/openid 的专用信号
        if (sourceID.contains("access_token") && sourceID.contains("openid")) {
            emit tokenCapturedFromConsole(sourceID);
        }
    }

signals:
    void tokenCapturedFromConsole(const QString &tokenMessage);
    void consoleMessageCaptured(const QString &fullMessage);
};

#endif // MYWEBPAGE_H
