#ifndef FSM_FRAMEWORK_H
#define FSM_FRAMEWORK_H

#include <QString>
#include <QMap>
#include <QList>
#include <functional>
#include <memory>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

namespace fsm {

// 前置声明
class State;
class StateMachine;
class Context;

// 状态转换结果
enum class TransitionResult {
    Success,        // 成功转换到下一状态
    Retry,          // 重试当前状态
    Failed,         // 失败，触发错误处理
    Completed       // 任务完成
};

// 转换条件类型
using TransitionCondition = std::function<bool(Context*)>;
using StateAction = std::function<TransitionResult(Context*)>;

// 任务上下文 - 存储任务执行中的所有数据
class Context {
public:
    // 通用数据存储
    QMap<QString, QVariant> data;

    // 重试计数器
    QMap<QString, int> retryCounters;

    // 时间追踪
    QElapsedTimer stateTimer;
    QElapsedTimer totalTimer;

    // 获取/设置数据的便捷方法
    template<typename T>
    T get(const QString& key, const T& defaultValue = T()) const {
        return data.value(key, defaultValue).value<T>();
    }

    void set(const QString& key, const QVariant& value) {
        data[key] = value;
    }

    // 重试管理
    int getRetryCount(const QString& stateName) const {
        return retryCounters.value(stateName, 0);
    }

    void incrementRetry(const QString& stateName) {
        retryCounters[stateName]++;
    }

    void resetRetry(const QString& stateName) {
        retryCounters[stateName] = 0;
    }
};

// 状态转换定义
struct Transition {
    QString targetState;           // 目标状态
    TransitionCondition condition;  // 转换条件(可选)
    QString description;            // 描述(用于日志)

    Transition(const QString& target,
               TransitionCondition cond = nullptr,
               const QString& desc = QString())
        : targetState(target), condition(cond), description(desc) {}
};

// 状态定义
class State {
public:
    QString name;                          // 状态名称
    StateAction onEnter;                   // 进入状态时执行
    StateAction onExecute;                 // 状态主逻辑
    StateAction onExit;                    // 退出状态时执行
    QMap<TransitionResult, Transition> transitions;  // 状态转换映射
    int maxRetries = 3;                    // 最大重试次数
    int retryDelayMs = 1000;              // 重试延迟

    State(const QString& stateName) : name(stateName) {}

    // 链式调用构建器
    State& setOnEnter(StateAction action) {
        onEnter = action;
        return *this;
    }

    State& setOnExecute(StateAction action) {
        onExecute = action;
        return *this;
    }

    State& setOnExit(StateAction action) {
        onExit = action;
        return *this;
    }

    State& addTransition(TransitionResult result,
                         const QString& targetState,
                         const QString& description = QString()) {
        transitions[result] = Transition(targetState, nullptr, description);
        return *this;
    }

    State& setMaxRetries(int retries) {
        maxRetries = retries;
        return *this;
    }
};

// 状态机
class StateMachine {
public:
    StateMachine(const QString& machineName = "FSM")
        : name_(machineName), running_(false) {}

    // 添加状态
    void addState(std::shared_ptr<State> state) {
        states_[state->name] = state;
    }

    // 设置初始状态
    void setInitialState(const QString& stateName) {
        initialState_ = stateName;
    }

    // 设置错误处理状态
    void setErrorState(const QString& stateName) {
        errorState_ = stateName;
    }

    // 设置完成状态
    void setCompletedState(const QString& stateName) {
        completedState_ = stateName;
    }

    // 运行状态机
    bool run(Context* context, std::function<bool()> shouldStop = nullptr) {
        if (!context) return false;

        running_ = true;
        currentState_ = initialState_;
        context->totalTimer.start();

        logCallback_(QString("[%1] 状态机启动，初始状态: %2")
                         .arg(name_).arg(currentState_));

        while (running_ && currentState_ != completedState_) {
            // 检查停止信号
            if (shouldStop && shouldStop()) {
                logCallback_(QString("[%1] 收到停止信号").arg(name_));
                return false;
            }

            // 获取当前状态
            auto state = states_.value(currentState_);
            if (!state) {
                logCallback_(QString("[%1] 错误：未找到状态 %2")
                                 .arg(name_).arg(currentState_));
                return false;
            }

            // 执行状态
            TransitionResult result = executeState(state, context);

            // 处理状态转换
            if (!handleTransition(state, result, context)) {
                return false;
            }
        }

        logCallback_(QString("[%1] 状态机完成，总耗时: %2ms")
                         .arg(name_).arg(context->totalTimer.elapsed()));
        return true;
    }

    // 设置日志回调
    void setLogCallback(std::function<void(const QString&)> callback) {
        logCallback_ = callback;
    }

    // 获取当前状态
    QString getCurrentState() const { return currentState_; }

private:
    TransitionResult executeState(std::shared_ptr<State> state, Context* context) {
        context->stateTimer.start();

        // 进入状态
        if (state->onEnter) {
            logCallback_(QString("[%1] → 进入状态: %2")
                             .arg(name_).arg(state->name));
            state->onEnter(context);
        }

        // 执行主逻辑
        TransitionResult result = TransitionResult::Failed;
        if (state->onExecute) {
            logCallback_(QString("[%1] ✓ 执行状态: %2")
                             .arg(name_).arg(state->name));
            result = state->onExecute(context);

            // 处理重试
            if (result == TransitionResult::Retry) {
                int retries = context->getRetryCount(state->name);
                if (retries < state->maxRetries) {
                    context->incrementRetry(state->name);
                    logCallback_(QString("[%1] ↻ 重试状态 %2 (%3/%4)")
                                     .arg(name_).arg(state->name)
                                     .arg(retries + 1).arg(state->maxRetries));
                    if (state->retryDelayMs > 0) {
                        QThread::msleep(state->retryDelayMs);
                    }
                    return executeState(state, context); // 递归重试
                } else {
                    logCallback_(QString("[%1] ✗ 状态 %2 重试次数已达上限")
                                     .arg(name_).arg(state->name));
                    result = TransitionResult::Failed;
                }
            }
        }

        // 退出状态
        if (state->onExit) {
            logCallback_(QString("[%1] ← 退出状态: %2 (耗时: %3ms)")
                             .arg(name_).arg(state->name)
                             .arg(context->stateTimer.elapsed()));
            state->onExit(context);
        }

        // 重置重试计数
        if (result != TransitionResult::Retry) {
            context->resetRetry(state->name);
        }

        return result;
    }

    bool handleTransition(std::shared_ptr<State> state,
                          TransitionResult result,
                          Context* context) {
        // 查找转换
        auto transition = state->transitions.find(result);
        if (transition == state->transitions.end()) {
            // 没有定义转换，使用默认处理
            if (result == TransitionResult::Failed && !errorState_.isEmpty()) {
                logCallback_(QString("[%1] ✗ 状态 %2 失败，转到错误处理")
                                 .arg(name_).arg(state->name));
                currentState_ = errorState_;
                return true;
            } else if (result == TransitionResult::Completed) {
                currentState_ = completedState_;
                return true;
            }

            logCallback_(QString("[%1] 错误：状态 %2 没有定义结果 %3 的转换")
                             .arg(name_).arg(state->name).arg((int)result));
            return false;
        }

        // 检查转换条件
        if (transition->condition && !transition->condition(context)) {
            logCallback_(QString("[%1] 转换条件不满足: %2")
                             .arg(name_).arg(transition->description));
            return false;
        }

        // 执行转换
        QString oldState = currentState_;
        currentState_ = transition->targetState;

        logCallback_(QString("[%1] 状态转换: %2 → %3 %4")
                         .arg(name_).arg(oldState).arg(currentState_)
                         .arg(transition->description.isEmpty() ? "" :
                                  QString("(%1)").arg(transition->description)));

        return true;
    }

private:
    QString name_;
    QMap<QString, std::shared_ptr<State>> states_;
    QString currentState_;
    QString initialState_;
    QString errorState_;
    QString completedState_;
    bool running_;
    std::function<void(const QString&)> logCallback_ = [](const QString& msg) {
        qDebug() << msg;
    };
};

} // namespace fsm

#endif // FSM_FRAMEWORK_H
