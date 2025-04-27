#include <iostream>
#include <string>
#include <functional>

#include "async.hpp"

struct MyService {
  auto step1() {
    return [this](auto next, auto _) {
      std::cout << "Step1: always succeed\n";
      next(Result<std::string, std::string>::Ok("ok_from_step1"));
    };
  }

  auto step2() {
    return [this](auto next, int attempt) {
      std::cout << "Attempt " << attempt << "\n";
      if (attempt < 2) {
        next(Result<std::string, std::string>::Err("fail"));
      } else {
        next(Result<std::string, std::string>::Ok("42"));
      }
    };
  }

  void final(Result<std::string, std::string> result) {
    if (result.is_ok()) {
      std::cout << "Final: success with value " << *result.value << "\n";
    } else {
      std::cout << "Final: failed with error " << *result.error << "\n";
    }
  }
};

void classLambdas() {
  MyService service;

  auto finalStep = [](auto result) {
    if (result.is_ok()) {
      std::cout << "Final Success: " << *result.value << "\n";
    } else {
      std::cout << "Final Failure: " << *result.error << "\n";
    }
  };

  auto step1 = service.step1();
  auto step2 = service.step2();

  initAsyncChain<std::string, std::string>()
      .then<std::string>(step1)
      .thenWithRetry<3, std::string>(step2) // 3 retries
      .finally(finalStep);
}

void lamdas() {
  std::cout << "Async Chain Example\n";
  using MyResult = Result<int, std::string>;

  int x = 23;
  auto step1 = [&x](auto next, auto result) {
    std::cout << "Step 1 OK\n";
    next(MyResult::Ok(x));
  };

  auto step2 = [](auto next, auto result) {
    std::cout << "step2: " << *result.value << "\n";
    std::cout << "Step 2 FAILS\n";
    next(Result<std::string, std::string>::Err("Step 2 error"));
  };

  auto catcher = [](auto next, auto error_result) {
    std::cout << "CatchError: recovered from " << *error_result.error << "\n";
    next(Result<std::string, std::string>::Ok("Recovered!"));
  };

  auto step3 = [](auto next, auto result) {
    std::cout << "Step 3 OK\n";
    std::cout << "Result: " << *result.value << "\n";
    next(Result<float, std::string>::Ok(3.14f));
  };


  auto finalStep = [](auto result) {
    if (result.is_ok()) {
      std::cout << "Final Success: " << *result.value << "\n";
    } else {
      std::cout << "Final Failure: " << *result.error << "\n";
    }
  };

  int failure_count = 0;
  auto stepWithRetry = [&failure_count](auto next, int trial) {
    std::cout << "Step with retry attempt " << trial << "\n";

    if (failure_count < 2) {
      // simulate failure on first 2 attempts
      failure_count++;
      next(Result<std::string, std::string>::Err("Simulated failure"));
    } else {
      failure_count = 0;
      next(Result<std::string, std::string>::Ok("Success after retries"));
    }
  };


  initAsyncChain<int, std::string>()
      .then<std::string>(step1)
      .then<std::string>(step2)
      .catchError(catcher)
      .then<float>(step3)
      .thenWithRetry<3, std::string>(stepWithRetry) // 3 retries
      .thenWithRetryDelayed<3, 1000, std::string>(stepWithRetry) // 3 retries, 500ms delay each
      .finally(finalStep);
}

void multilevelChain() {
  auto step1 = [](auto next, auto result) {
    std::cout << "Outer Step 1 OK\n";
    next(Result<std::string, std::string>::Ok("OK"));
  };
  auto step2 = [](auto next, auto result) {
    std::cout << "Outer Step 2 processing\n";

    auto s1 = [](auto next, auto result) {
      std::cout << "inner Step 1 OK\n";
      next(Result<std::string, std::string>::Ok("OK from internal s1"));
    };

    auto s2 = [](auto next, auto result) {
      std::cout << "inner Step 2 OK\n";
      next(Result<std::string, std::string>::Err("OK from internal s2"));
    };

    auto final = [&next](auto result) {
      std::cout << "inner chain done\n";
      next(result);
    };

    initAsyncChain<std::string, std::string>()
        .template then<std::string>(s1)
        .template then<std::string>(s2)
        .finally(final);
  };

  auto final = [](auto result) {
    if (result.is_ok()) {
      std::cout << "Final Success with msg " << *result.value << "\n";
    } else {
      std::cout << "Final Failure with msg " << *result.error << "\n";
    }
    std::cout << "out chain invoked done!\n";
  };


  initAsyncChain<std::string, std::string>()
      .then<std::string>(step1)
      .then<std::string>(step2)
      .finally(final);
}

int main() {
  MyService service;

  // Set fake scheduler (execute immediately, no real delay)
  setScheduler([](std::function<void()> task, std::size_t delay_ms) {
    std::cout << "Scheduled with fake delay " << delay_ms << "ms\n";
    task();
  });

  std::cout << "<------ class lambda example\n";
  classLambdas();
  std::cout << "<------ functional lambda example\n";
  lamdas();
  std::cout << "<------ multi level chain example\n";
  multilevelChain();
  return 0;
}
