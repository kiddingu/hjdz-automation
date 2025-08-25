#include <QWebEnginePage>

class SilentWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    using QWebEnginePage::QWebEnginePage;
protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override {
        Q_UNUSED(level)
        Q_UNUSED(message)
        Q_UNUSED(lineNumber)
        Q_UNUSED(sourceID)
        // 不做任何输出，屏蔽网页 console 日志
    }
};
