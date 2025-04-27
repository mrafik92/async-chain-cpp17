#ifndef WORKSPACES_CPP20_ASYNC_HPP
#define WORKSPACES_CPP20_ASYNC_HPP

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace async_chain {

using SchedulerFunction =
    std::function<void(std::function<void()>, std::size_t delay_ms)>;

inline SchedulerFunction global_scheduler;

inline void setScheduler(SchedulerFunction scheduler) {
  global_scheduler = std::move(scheduler);
}

template <typename T, typename E = std::string>
struct Result {
  using ValueType = T;
  std::optional<E> error;
  std::optional<T> value;

  static auto Ok(T val) -> Result {
    return Result{std::nullopt, std::move(val)};
  }

  static auto Err(E err) -> Result {
    return Result{std::move(err), std::nullopt};
  }

  [[nodiscard]] auto is_ok() const -> bool { return value.has_value(); }
  [[nodiscard]] auto is_err() const -> bool { return error.has_value(); }
};

template <typename E>
struct Result<void, E> {
  std::optional<E> error;

  static auto Ok() -> Result { return Result{std::nullopt}; }
  static auto Err(E err) -> Result { return Result{std::move(err)}; }
  [[nodiscard]] auto is_ok() const -> bool { return !error.has_value(); }
  [[nodiscard]] auto is_err() const -> bool { return error.has_value(); }
};

// Exported type aliases for user convenience

template <typename T, typename E = std::string>
using ResultType = Result<T, E>;

template <typename T, typename E = std::string>
using StepType =
    std::function<void(std::function<void(Result<T, E>)>, Result<T, E>)>;

// StepTypeWithAttempts: for steps that receive the number of attempts (used by RetryHolder)
template <typename T, typename E = std::string>
using StepTypeWithAttempts = std::function<void(
    std::function<void(Result<T, E>)>, std::size_t, Result<T, E>)>;

template <typename T, typename E = std::string>
using NextType = std::function<void(Result<T, E>)>;

template <typename Step>
struct Holder {
  Step* ptr;

  explicit Holder(Step& ref) : ptr(&ref) {
    static_assert(!std::is_reference_v<Step>,
                  "Holder should not be used with reference types");
  }

  auto get() -> Step& { return *ptr; }

  template <typename Continue, typename CurrentResult>
  void call(Continue&& continue_chain, CurrentResult&& result) {
    if (result.is_err()) {
      continue_chain(std::forward<CurrentResult&&>(result));
      return;
    }
    (*ptr)(
        [continue_chain = std::forward<Continue&&>(continue_chain)](
            auto next_result) mutable {
          continue_chain(std::move(next_result));
        },
        std::forward<CurrentResult&&>(result));
  }
};

template <typename Step>
struct CatcherHolder {
  Step* ptr;

  explicit CatcherHolder(Step& ref) : ptr(&ref) {
    static_assert(!std::is_reference_v<Step>,
                  "CatcherHolder should not be used with reference types");
  }

  auto get() -> Step& { return *ptr; }

  template <typename Continue, typename CurrentResult>
  void call(Continue&& continue_chain, CurrentResult&& result) {
    if (result.is_err()) {
      (*ptr)(
          [continue_chain = std::forward<Continue&&>(continue_chain)](
              auto recovered_result) mutable {
            continue_chain(std::move(recovered_result));
          },
          std::forward<CurrentResult&&>(result));
    } else {
      continue_chain(std::forward<CurrentResult&&>(result));
    }
  }
};

template <std::size_t MaxRetries, typename Step>
struct RetryHolder {

  explicit RetryHolder(Step& ref) : ptr(&ref) {
    static_assert(!std::is_reference_v<Step>,
                  "RetryHolder should not be used with reference types");
  }

  auto get() -> Step& { return *ptr; }

  template <typename Continue, typename CurrentResult>
  void call(Continue&& continue_chain, CurrentResult&& result) {
    if (result.is_err()) {
      continue_chain(std::forward<CurrentResult&&>(result));
      return;
    }
    run_step(std::forward<Continue&&>(continue_chain), 0);
  }

 private:
  Step* ptr;

  template <typename Continue>
  void run_step(Continue&& continue_chain, std::size_t attempt) {
    (*ptr)(
        [continue_chain = std::forward<Continue>(continue_chain), this,
         attempt](auto result) mutable {
          if (result.is_ok() || attempt >= MaxRetries) {
            continue_chain(std::move(result));
          } else {
            run_step(std::move(continue_chain), attempt + 1);
          }
        },
        attempt);
  }
};

template <std::size_t MaxRetries, std::size_t DelayMs, typename Step>
struct RetryDelayedHolder {

  explicit RetryDelayedHolder(Step& ref) : ptr(&ref) {
    static_assert(!std::is_reference_v<Step>,
                  "RetryDelayedHolder should not be used with reference types");
  }

  auto get() -> Step& { return *ptr; }

  template <typename Continue, typename CurrentResult>
  void call(Continue&& continue_chain, CurrentResult&& result) {
    if (result.is_err()) {
      continue_chain(std::forward<decltype(result)>(result));
      return;
    }
    run_step(std::forward<Continue>(continue_chain), 0);
  }

 private:
  Step* ptr;

  template <typename Continue>
  void run_step(Continue&& next, size_t attempt) {
    (*ptr)(
        [continue_chain = std::forward<Continue>(next), attempt,
         this](auto result) mutable {
          if (result.is_ok() || attempt >= MaxRetries) {
            continue_chain(std::move(result));
          } else {
            global_scheduler(
                [this, continue_chain = std::move(continue_chain),
                 attempt]() mutable {
                  run_step(std::move(continue_chain), attempt + 1);
                },
                DelayMs);
          }
        },
        attempt);
  }
};

template <typename T, typename E, typename... StepHolders>
class AsyncChain {
 public:
  AsyncChain(const AsyncChain&) = delete;
  AsyncChain(AsyncChain&&) = delete;
  auto operator=(const AsyncChain&) -> AsyncChain& = delete;
  auto operator=(AsyncChain&&) -> AsyncChain& = delete;
  ~AsyncChain() = default;

  explicit AsyncChain(StepHolders&&... holders)
      : steps_(std::forward<StepHolders>(holders)...) {}

  template <typename... OldSteps, typename NewStep>
  AsyncChain(std::tuple<OldSteps...>&& old_steps, NewStep&& new_step)
      : steps_(
            std::tuple_cat(std::move(old_steps),
                           std::make_tuple(std::forward<NewStep>(new_step)))) {}

  template <typename Step>
  auto then(Step&& step) && {
    using NewHolder = Holder<std::decay_t<Step>>;
    return AsyncChain<T, E, StepHolders..., NewHolder>(
        std::move(steps_), NewHolder(std::forward<Step>(step)));
  }

  template <std::size_t MaxRetries, typename Step>
  auto thenWithRetry(Step&& step) && {
    using NewHolder = RetryHolder<MaxRetries, std::decay_t<Step>>;
    return AsyncChain<T, E, StepHolders..., NewHolder>(
        std::move(steps_), NewHolder(std::forward<Step>(step)));
  }

  template <typename Catcher>
  auto catchError(Catcher&& catcher) && {
    using CatchHolder = CatcherHolder<std::decay_t<Catcher>>;
    return AsyncChain<T, E, StepHolders..., CatchHolder>(
        std::move(steps_), CatchHolder(std::forward<Catcher>(catcher)));
  }

  template <std::size_t MaxRetries, std::size_t DelayMs, typename Step>
  auto thenWithRetryDelayed(Step&& step) && {
    using NewHolder =
        RetryDelayedHolder<MaxRetries, DelayMs, std::decay_t<Step>>;
    return AsyncChain<T, E, StepHolders..., NewHolder>(
        std::move(steps_), NewHolder(std::forward<Step>(step)));
  }

  template <typename FinalCallback>
  void finally(FinalCallback&& final_callback) && {
    call_steps<0>(std::move(steps_),
                  std::forward<FinalCallback>(final_callback),
                  Result<T, E>::Ok(T{}));
  }

 private:
  std::tuple<StepHolders...> steps_;

  template <std::size_t Index, typename StepsTuple, typename FinalCallback,
            typename CurrentResult>
  static void call_steps(StepsTuple&& steps, FinalCallback&& final_callback,
                         CurrentResult&& result) {
    if constexpr (Index < std::tuple_size_v<std::decay_t<StepsTuple>>) {
      auto& holder = std::get<Index>(steps);

      // Define a small continuation
      auto continue_chain = [steps = std::forward<StepsTuple>(steps),
                             final_callback = std::forward<FinalCallback>(
                                 final_callback)](auto&& next_result) mutable {
        call_steps<Index + 1>(std::move(steps), std::move(final_callback),
                              std::forward<decltype(next_result)>(next_result));
      };

      holder.call(std::forward<decltype(continue_chain)>(continue_chain),
                  std::forward<CurrentResult>(result));
    } else {
      final_callback(std::forward<CurrentResult>(result));
    }
  }
};

template <typename T, typename E>
auto initAsyncChain() {
  return AsyncChain<T, E>{};
}

}  // namespace async_chain

#endif