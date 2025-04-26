#include <tuple>
#include <optional>
#include <utility>
#include <functional>
#include <type_traits>
#include <string>
#include <iostream>
#include <thread>

using SchedulerFunction = std::function<void(std::function<void()>, std::size_t delay_ms)>;

inline SchedulerFunction global_scheduler;

inline void setScheduler(SchedulerFunction scheduler)
{
    global_scheduler = std::move(scheduler);
}

// forward declaration of AsyncChainSmart
template <typename T, typename E, typename... StepHolders>
class AsyncChainSmart;

template <typename T, typename E = std::string>
struct Result
{
    using ValueType = T;
    std::optional<E> error;
    std::optional<T> value;

    static Result Ok(T val)
    {
        return Result{std::nullopt, std::move(val)};
    }

    static Result Err(E err)
    {
        return Result{std::move(err), std::nullopt};
    }

    bool is_ok() const { return value.has_value(); }
    bool is_err() const { return error.has_value(); }
};

template <typename Step>
struct Holder
{
    Step value;

    template <typename U>
    Holder(U &&u) : value(std::forward<U>(u)) {}

    Step &get() { return value; }

    template <typename Continue, typename CurrentResult>
    void call(Continue &&continue_chain, CurrentResult &&result)
    {
        if (result.is_err())
        {
            continue_chain(std::move(result));
            return;
        }
        value([continue_chain = std::move(continue_chain)](auto next_result) mutable
              { continue_chain(std::move(next_result)); });
    }
};

template <typename Step>
struct Holder<Step &>
{
    Step *ptr;

    Holder(Step &ref) : ptr(&ref) {}

    Step &get() { return *ptr; }

    template <typename Continue, typename CurrentResult>
    void call(Continue &&continue_chain, CurrentResult &&result)
    {
        if (result.is_err())
        {
            continue_chain(std::move(result));
            return;
        }
        (*ptr)([continue_chain = std::move(continue_chain)](auto next_result) mutable
               { continue_chain(std::move(next_result)); });
    }
};

// Default: assume it is not a catcher
template <typename T>
struct is_catcher : std::false_type
{
};

// Specialized for functions taking 2 arguments (next, error_result)
template <typename F>
struct is_catcher<Holder<F>>
{
    static constexpr bool value = false;
};

template <typename T>
struct is_retry : std::false_type
{
};

// CatcherHolder for value
template <typename Step>
struct CatcherHolder
{
    Step value;

    template <typename U>
    CatcherHolder(U &&u) : value(std::forward<U>(u)) {}

    Step &get() { return value; }

    template <typename Continue, typename CurrentResult>
    void call(Continue &&continue_chain, CurrentResult &&result)
    {
        if (result.is_err())
        {
            value(
                [continue_chain = std::move(continue_chain)](auto recovered_result) mutable
                {
                    continue_chain(std::move(recovered_result));
                },
                std::move(result));
        }
        else
        {
            continue_chain(std::move(result));
        }
    }
};

// CatcherHolder specialization for reference
template <typename Step>
struct CatcherHolder<Step &>
{
    Step *ptr;

    CatcherHolder(Step &ref) : ptr(&ref) {}

    Step &get() { return *ptr; }

    template <typename Continue, typename CurrentResult>
    void call(Continue &&continue_chain, CurrentResult &&result)
    {
        if (result.is_err())
        {
            (*ptr)(
                [continue_chain = std::move(continue_chain)](auto recovered_result) mutable
                {
                    continue_chain(std::move(recovered_result));
                },
                std::move(result));
        }
        else
        {
            continue_chain(std::move(result));
        }
    }
};

template <typename T>
struct is_catcher<CatcherHolder<T>> : std::true_type
{
};

// RetryHolder for value
template <std::size_t MaxRetries, typename Step>
struct RetryHolder
{
    Step step;

    template <typename U>
    RetryHolder(U &&u) : step(std::forward<U>(u)) {}

    Step &get() { return step; }

    template <typename Continue, typename CurrentResult>
    void call(Continue &&continue_chain, CurrentResult &&result)
    {
        if (result.is_err())
        {
            continue_chain(std::move(result));
            return;
        }
        run_step(std::move(continue_chain), 0);
    }

private:
    template <typename Continue>
    void run_step(Continue &&continue_chain, std::size_t attempt)
    {
        step(
            [continue_chain = std::move(continue_chain), this, attempt](auto result) mutable
            {
                if (result.is_ok() || attempt >= MaxRetries)
                {
                    continue_chain(std::move(result));
                }
                else
                {
                    run_step(std::move(continue_chain), attempt + 1);
                }
            },
            attempt);
    }
};

// RetryHolder specialization for reference
template <std::size_t MaxRetries, typename Step>
struct RetryHolder<MaxRetries, Step &>
{
    Step *ptr;

    RetryHolder(Step &ref) : ptr(&ref) {}

    Step &get() { return *ptr; }

    template <typename Continue, typename CurrentResult>
    void call(Continue &&continue_chain, CurrentResult &&result)
    {
        if (result.is_err())
        {
            continue_chain(std::move(result));
            return;
        }
        run_step(std::move(continue_chain), 0);
    }

private:
    template <typename Continue>
    void run_step(Continue &&continue_chain, std::size_t attempt)
    {
        (*ptr)(
            [continue_chain = std::move(continue_chain), this, attempt](auto result) mutable
            {
                if (result.is_ok() || attempt >= MaxRetries)
                {
                    continue_chain(std::move(result));
                }
                else
                {
                    run_step(std::move(continue_chain), attempt + 1);
                }
            },
            attempt);
    }
};

template <std::size_t MaxRetries, typename Step>
struct is_retry<RetryHolder<MaxRetries, Step>> : std::true_type
{
};

template <std::size_t MaxRetries, std::size_t DelayMs, typename Step>
struct RetryDelayedHolder
{
    Step step;

    template <typename U>
    RetryDelayedHolder(U &&u) : step(std::forward<U>(u)) {}

    Step &get() { return step; }

    template <typename Continue, typename CurrentResult>
    void call(Continue &&continue_chain, CurrentResult &&result)
    {
        if (result.is_err())
        {
            continue_chain(std::move(result));
            return;
        }
        run_step(std::move(continue_chain), 0);
    }

private:
    template <typename Continue>
    void run_step(Continue &&next, size_t attempt)
    {
        step(
            [continue_chain = std::forward<Continue>(next), attempt, this](auto result) mutable
            {
                if (result.is_ok() || attempt >= MaxRetries)
                {
                    continue_chain(std::move(result));
                }
                else
                {
                    global_scheduler(
                        [this, continue_chain = std::move(continue_chain), attempt]() mutable
                        {
                            run_step(std::move(continue_chain), attempt + 1);
                        },
                        DelayMs);
                }
            },
            attempt);
    }
};

template <std::size_t MaxRetries, std::size_t DelayMs, typename Step>
struct RetryDelayedHolder<MaxRetries, DelayMs, Step &>
{
    Step *ptr;

    RetryDelayedHolder(Step &ref) : ptr(&ref) {}

    Step &get() { return *ptr; }

    template <typename Continue, typename CurrentResult>
    void call(Continue &&continue_chain, CurrentResult &&result)
    {
        if (result.is_err())
        {
            continue_chain(std::move(result));
            return;
        }
        run_step(std::move(continue_chain), 0);
    }

private:
    template <typename Continue>
    void run_step(Continue &&next, size_t attempt)
    {
        (*ptr)(
            [continue_chain = std::forward<Continue>(next), attempt, this](auto result) mutable
            {
                if (result.is_ok() || attempt >= MaxRetries)
                {
                    continue_chain(std::move(result));
                }
                else
                {
                    global_scheduler(
                        [this, continue_chain = std::move(continue_chain), attempt]() mutable
                        {
                            run_step(std::move(continue_chain), attempt + 1);
                        },
                        DelayMs);
                }
            },
            attempt);
    }
};

template <typename T, typename E, typename... StepHolders>
class AsyncChainSmart
{
public:
    explicit AsyncChainSmart(StepHolders &&...holders)
        : steps_(std::forward<StepHolders>(holders)...) {}

    template <typename... OldSteps, typename NewStep>
    AsyncChainSmart(std::tuple<OldSteps...> &&old_steps, NewStep &&new_step)
        : steps_(std::tuple_cat(std::move(old_steps), std::make_tuple(std::forward<NewStep>(new_step)))) {}

    template <typename NewT, typename NewStep>
    auto then(NewStep &&new_step) &&
    {
        using NewHolder = Holder<std::decay_t<NewStep>>;
        return AsyncChainSmart<NewT, E, StepHolders..., NewHolder>(
            std::move(steps_), NewHolder(std::forward<NewStep>(new_step)));
    }

    template <std::size_t MaxRetries, typename NewT, typename NewStep>
    auto thenWithRetry(NewStep &&new_step) &&
    {
        using NewHolder = RetryHolder<MaxRetries, std::decay_t<NewStep>>;
        return AsyncChainSmart<NewT, E, StepHolders..., NewHolder>(
            std::move(steps_), NewHolder(std::forward<NewStep>(new_step)));
    }

    template <typename Catcher>
    auto catchError(Catcher &&catcher) &&
    {
        using CatchHolder = CatcherHolder<std::decay_t<Catcher>>;
        return AsyncChainSmart<T, E, StepHolders..., CatchHolder>(
            std::move(steps_), CatchHolder(std::forward<Catcher>(catcher)));
    }

    template <std::size_t MaxRetries, std::size_t DelayMs, typename NewT, typename NewStep>
    auto thenWithRetryDelayed(NewStep &&new_step) &&
    {
        using NewHolder = RetryDelayedHolder<MaxRetries, DelayMs, std::decay_t<NewStep>>;
        return AsyncChainSmart<NewT, E, StepHolders..., NewHolder>(
            std::move(steps_), NewHolder(std::forward<NewStep>(new_step)));
    }

    template <typename FinalCallback>
    void finally(FinalCallback &&final_callback) &&
    {
        call_steps<0>(std::move(steps_), std::forward<FinalCallback>(final_callback), Result<T, E>::Ok(T{}));
    }

private:
    std::tuple<StepHolders...> steps_;

    template <std::size_t Index, typename StepsTuple, typename FinalCallback, typename CurrentResult>
    static void call_steps(StepsTuple &&steps, FinalCallback &&final_callback, CurrentResult &&result)
    {
        if constexpr (Index < std::tuple_size_v<std::decay_t<StepsTuple>>)
        {
            auto &holder = std::get<Index>(steps);

            // Define a small continuation
            auto continue_chain = [steps = std::move(steps), final_callback = std::move(final_callback)](auto &&next_result) mutable
            {
                call_steps<Index + 1>(std::move(steps), std::move(final_callback), std::forward<decltype(next_result)>(next_result));
            };

            holder.call(std::move(continue_chain), std::move(result));
        }
        else
        {
            final_callback(std::move(result));
        }
    }
};

template <typename T, typename E, typename... Steps>
auto callAllSmart(Steps &&...steps)
{
    return AsyncChainSmart<T, E, Holder<std::decay_t<Steps>>...>(Holder<std::decay_t<Steps>>(std::forward<Steps>(steps))...);
}
