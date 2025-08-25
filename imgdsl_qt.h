#ifndef IMGDSL_QT_H
#define IMGDSL_QT_H
#pragma once

#include <QString>
#include <QPoint>
#include <QRect>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <vector>
#include <memory>
#include <functional>
#include <QStringList>

namespace imgdsl {

struct MatchResult {
    bool matched{false};
    QPoint point;
    double score{0.0};
    QString which;
};

// =============== 工具接口（需由上层实现并注入） ===============
struct IToolbox {
    virtual ~IToolbox() = default;
    virtual MatchResult findImage(const QString& path, double th,
                                  const QRect& roi, bool multiScale) = 0;
    virtual bool clickLogical(const QPoint& logicalPt) = 0;
    virtual void sleepMs(int ms) = 0;
    virtual void logAction(const QString& action, const QString& conditionName, int timeout = -1, const imgdsl::MatchResult* result = nullptr) = 0;
    virtual void logInfo(const QString& message) = 0;
    virtual void logError(const QString& message) = 0;
    virtual void logSuccess(const QString& message) = 0;

    // 【新增】任务上下文管理
    virtual void setTaskContext(const QString& taskName) = 0;
    virtual void clearTaskContext() = 0;
    virtual QString resolveImagePath(const QString& imageNameOrPath) const = 0;
};

inline IToolbox*& toolbox() {
    static IToolbox* g = nullptr;
    return g;
}

inline void set_toolbox(IToolbox* t) { toolbox() = t; }

// =============== 条件抽象 ===============
class Condition {
public:
    using EvalFn = std::function<MatchResult()>;
    Condition() = default;
    explicit Condition(EvalFn fn, QString name = {})
        : eval_(std::move(fn)), name_(std::move(name)) {}

    bool eval(MatchResult* out = nullptr) const {
        MatchResult r = eval_ ? eval_() : MatchResult{};
        last_ = r;
        if (out) *out = r;
        return r.matched;
    }

    operator bool() const { return eval(); }

    const MatchResult& last() const { return last_; }

    bool click() const {
        if (!toolbox()) { qWarning() << "[imgdsl] toolbox not set"; return false; }
        if (!last_.matched) return false;
        if (toolbox()) toolbox()->logAction("CLICK", name(), -1, &last_);
        return toolbox()->clickLogical(last_.point);
    }

    QString name() const { return name_; }

private:
    EvalFn eval_{};
    mutable MatchResult last_{};
    QString name_{};

public:
    static Condition APPEAR(QString path, double th = 0.85,
                            QRect roi = QRect(), bool multiScale = true) {
        return Condition([=]() -> MatchResult {
            if (!toolbox()) { qWarning() << "[imgdsl] toolbox not set"; return {}; }
            auto r = toolbox()->findImage(path, th, roi, multiScale);
            if (r.matched && r.which.isEmpty()) r.which = path;
            return r;
        }, QString("APPEAR(%1)").arg(path));
    }

    static Condition NOT(Condition c) {
        return Condition([=]() -> MatchResult {
            MatchResult inner;
            bool ok = c.eval(&inner);
            MatchResult out;
            out.matched = !ok;
            out.which = QString("NOT(%1)").arg(c.name());
            return out;
        }, QString("NOT(%1)").arg(c.name()));
    }

    static Condition ANY(std::vector<Condition> conds) {
        QStringList names;
        for (const auto& c : conds) { names << c.name(); }
        return Condition([=]() -> MatchResult {
            for (const auto& c : conds) {
                MatchResult r;
                if (c.eval(&r)) return r;
            }
            return {};
        }, QString("ANY(%1)").arg(names.join(" | ")));
    }

    static Condition ALL(std::vector<Condition> conds) {
        QStringList names;
        for (const auto& c : conds) { names << c.name(); }
        return Condition([=]() -> MatchResult {
            MatchResult first;
            bool firstFilled = false;
            for (const auto& c : conds) {
                MatchResult r;
                if (!c.eval(&r)) return {};
                if (!firstFilled) { first = r; firstFilled = true; }
            }
            return first;
        }, QString("ALL(%1)").arg(names.join(" & ")));
    }

    static Condition STABILIZED(Condition c, int n = 2, int intervalMs = 150) {
        return Condition([=]() -> MatchResult {
            if (!toolbox()) return {};
            MatchResult last;
            for (int i = 0; i < n; ++i) {
                if (!c.eval(&last)) return {};
                if (i < n - 1) toolbox()->sleepMs(intervalMs);
            }
            return last;
        }, QString("STABILIZED(%1,x%2)").arg(c.name()).arg(n));
    }
};

// 【优化点】IMG 函数现在会自动解析路径
inline Condition IMG(const QString& imageNameOrPath, double th = 0.85,
                     QRect roi = QRect(), bool multiScale = true) {
    if (!toolbox()) {
        qWarning() << "[imgdsl] toolbox not set, cannot resolve image path";
        return Condition::APPEAR(imageNameOrPath, th, roi, multiScale);
    }
    QString fullPath = toolbox()->resolveImagePath(imageNameOrPath);
    return Condition::APPEAR(fullPath, th, roi, multiScale);
}

// 逻辑运算（与/或/非）
inline Condition operator||(const Condition& a, const Condition& b) {
    return Condition::ANY({a, b});
}
inline Condition operator&&(const Condition& a, const Condition& b) {
    return Condition::ALL({a, b});
}
inline Condition operator!(const Condition& a) { return Condition::NOT(a); }

template <typename... Cs>
Condition ANY(Cs... cs) { return Condition::ANY({cs...}); }

template <typename... Cs>
Condition ALL(Cs... cs) { return Condition::ALL({cs...}); }

// 等待直到条件成立（带超时/轮询间隔）
inline bool WAIT_UNTIL(const Condition& c, int timeoutMs = 8000, int intervalMs = 200,
                       MatchResult* out = nullptr) {
    if (!toolbox()) { qWarning() << "[imgdsl] toolbox not set"; return false; }
    toolbox()->logAction("WAIT_UNTIL", c.name(), timeoutMs);
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() <= timeoutMs) {
        MatchResult r;
        if (c.eval(&r)) { if (out) *out = r; return true; }
        toolbox()->sleepMs(intervalMs);
    }
    return false;
}

inline bool CLICK(const Condition& c) { return c.click(); }

inline bool CLICK_AT(const QPoint& pt) {
    if (!toolbox()) return false;
    return toolbox()->clickLogical(pt);
}

inline void SLEEP(int ms) {
    if (!toolbox()) { QThread::msleep(static_cast<unsigned long>(ms)); return; }
    toolbox()->sleepMs(ms);
}

}
#endif // IMGDSL_QT_H
