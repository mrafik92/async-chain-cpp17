#include <gtest/gtest.h>
#include <vector>
#include <iostream>
#include <string>

#include "async.hpp"

using namespace async_chain;

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

struct MyData {
    int count;
    double value;
    std::string message;
};

TEST(AsyncChainTest, Placeholder) {
    std::cout << "hello Placeholder test\n";
    // Placeholder test, will add more tests later
    SUCCEED();
}

TEST(AsyncChainTest, TwoStepsAndFinally) {
    using MyResult = Result<int, std::string>;
    int step1_value = 0;
    int step2_value = 0;
    int final_value = 0;
    std::string final_error;
    bool final_ok = false;

    auto step1 = [&step1_value](auto next, MyResult result) {
        step1_value = 10;
        next(MyResult::Ok(step1_value));
    };

    auto step2 = [&step2_value](auto next, MyResult result) {
        EXPECT_TRUE(result.is_ok());
        step2_value = *result.value + 5;
        next(MyResult::Ok(step2_value));
    };

    auto finalStep = [&](MyResult result) {
        if (result.is_ok()) {
            final_ok = true;
            final_value = *result.value;
        } else {
            final_ok = false;
            final_error = *result.error;
        }
    };

    initAsyncChain<int, std::string>()
        .then(step1)
        .then(step2)
        .finally(finalStep);

    EXPECT_EQ(step1_value, 10);
    EXPECT_EQ(step2_value, 15);
    EXPECT_TRUE(final_ok);
    EXPECT_EQ(final_value, 15);
}

TEST(AsyncChainTest, ThreeSteps_SecondFails_InvokesFinallyWithError) {
    using MyResult = Result<int, std::string>;
    int step1_value = 0;
    int step2_value = 0;
    int step3_value = 0;
    std::string final_error;
    bool final_ok = false;
    int final_value = 0;

    auto step1 = [&step1_value](auto next, MyResult result) {
        step1_value = 5;
        next(MyResult::Ok(step1_value));
    };

    auto step2 = [&step2_value](auto next, MyResult result) {
        EXPECT_TRUE(result.is_ok());
        step2_value = *result.value + 10;
        // Simulate failure
        next(MyResult::Err("Step 2 failed"));
    };

    auto step3 = [&step3_value](auto next, MyResult result) {
        // This should not be called
        step3_value = *result.value + 100;
        next(MyResult::Ok(step3_value));
    };

    auto finalStep = [&](MyResult result) {
        if (result.is_ok()) {
            final_ok = true;
            final_value = *result.value;
        } else {
            final_ok = false;
            final_error = *result.error;
        }
    };

    initAsyncChain<int, std::string>()
        .then(step1)
        .then(step2)
        .then(step3)
        .finally(finalStep);

    EXPECT_EQ(step1_value, 5);
    EXPECT_EQ(step2_value, 15);
    EXPECT_EQ(step3_value, 0); // step3 should not be called
    EXPECT_FALSE(final_ok);
    EXPECT_EQ(final_error, "Step 2 failed");
}

TEST(AsyncChainTest, FifteenSteps_FourthFails) {
    using MyResult = Result<int, std::string>;
    int values[15] = {0};
    int final_value = 0;
    std::string final_error;
    bool final_ok = false;

    std::vector<async_chain::StepType<int>> steps;
    steps.reserve(15);

    for (int i = 0; i < 15; ++i) {
        steps.emplace_back([&, i](auto next, MyResult result) {
            if (i == 0) {
                values[i] = 1;
                next(MyResult::Ok(values[i]));
            } else if (i == 3) {
                values[i] = values[i-1] + 1;
                next(MyResult::Err("Step 4 failed"));
            } else {
                if (result.is_ok()) {
                    values[i] = *result.value + 1;
                    next(MyResult::Ok(values[i]));
                } else {
                    next(result);
                }
            }
        });
    }

    auto finalStep = [&](MyResult result) {
        if (result.is_ok()) {
            final_ok = true;
            final_value = *result.value;
        } else {
            final_ok = false;
            final_error = *result.error;
        }
    };

    initAsyncChain<int, std::string>()
        .then(steps[0])
        .then(steps[1])
        .then(steps[2])
        .then(steps[3])
        .then(steps[4])
        .then(steps[5])
        .then(steps[6])
        .then(steps[7])
        .then(steps[8])
        .then(steps[9])
        .then(steps[10])
        .then(steps[11])
        .then(steps[12])
        .then(steps[13])
        .then(steps[14])
        .finally(finalStep);

    // Only steps 0-3 should be executed, step 4 and beyond should not
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
    EXPECT_EQ(values[3], 4);
    for (int i = 4; i < 15; ++i) {
        EXPECT_EQ(values[i], 0);
    }
    EXPECT_FALSE(final_ok);
    EXPECT_EQ(final_error, "Step 4 failed");
}

TEST(AsyncChainTest, StructResultEachStepModifiesField) {
    using MyResult = Result<MyData, std::string>;
    MyData initial{0, 0.0, ""};
    MyData final_data;
    bool final_ok = false;

    auto step1 = [](auto next, MyResult) {
        MyData d{1, 0.0, ""};
        next(MyResult::Ok(d));
    };
    auto step2 = [](auto next, MyResult result) {
        MyData d = *result.value;
        d.value = 3.14;
        next(MyResult::Ok(d));
    };
    auto step3 = [](auto next, MyResult result) {
        MyData d = *result.value;
        d.message = "done";
        next(MyResult::Ok(d));
    };
    auto finalStep = [&](MyResult result) {
        final_ok = result.is_ok();
        if (final_ok) final_data = *result.value;
    };

    initAsyncChain<MyData, std::string>()
        .then(step1)
        .then(step2)
        .then(step3)
        .finally(finalStep);

    EXPECT_TRUE(final_ok);
    EXPECT_EQ(final_data.count, 1);
    EXPECT_DOUBLE_EQ(final_data.value, 3.14);
    EXPECT_EQ(final_data.message, "done");
}

TEST(AsyncChainDemo, ClassLambdas) {
    MyService service;
    std::string final_result;
    bool final_ok = false;
    auto step1 = service.step1();
    auto step2 = service.step2();
    auto finalStep = [&](Result<std::string, std::string> result) {
        if (result.is_ok()) {
            final_ok = true;
            final_result = *result.value;
        } else {
            final_ok = false;
            final_result = *result.error;
        }
    };
    setScheduler([](std::function<void()> task, std::size_t) { task(); });
    initAsyncChain<std::string, std::string>()
        .then(step1)
        .thenWithRetry<3>(step2)
        .finally(finalStep);
    EXPECT_TRUE(final_ok);
    EXPECT_EQ(final_result, "42");
}

TEST(AsyncChainDemo, Lambdas) {
    using MyResult = Result<int, std::string>;
    int x = 23;
    int step1_val = 0;
    std::string catch_error;
    int step3_val = 0;
    std::string final_result;
    bool final_ok = false;
    int failure_count = 0;
    auto step1 = [&x, &step1_val](auto next, MyResult) {
        step1_val = x;
        next(MyResult::Ok(x));
    };
    auto step2 = [](auto next, MyResult) {
        next(MyResult::Err("Step 2 error"));
    };
    auto catcher = [&catch_error](auto next, MyResult error_result) {
        catch_error = *error_result.error;
        next(MyResult::Ok(0));
    };
    auto step3 = [&step3_val](auto next, MyResult) {
        step3_val = 314;
        next(MyResult::Ok(step3_val));
    };
    auto stepWithRetry = [&failure_count](auto next, int trial) {
        if (failure_count < 2) {
            failure_count++;
            next(MyResult::Err("Simulated failure"));
        } else {
            failure_count = 0;
            next(MyResult::Ok(42));
        }
    };
    auto finalStep = [&](MyResult result) {
        if (result.is_ok()) {
            final_ok = true;
            final_result = std::to_string(*result.value);
        } else {
            final_ok = false;
            final_result = *result.error;
        }
    };
    setScheduler([](std::function<void()> task, std::size_t) { task(); });
    initAsyncChain<int, std::string>()
        .then(step1)
        .then(step2)
        .catchError(catcher)
        .then(step3)
        .thenWithRetry<3>(stepWithRetry)
        .thenWithRetryDelayed<3, 1000>(stepWithRetry)
        .finally(finalStep);
    EXPECT_EQ(step1_val, 23);
    EXPECT_EQ(catch_error, "Step 2 error");
    EXPECT_EQ(step3_val, 314);
    EXPECT_TRUE(final_ok);
    EXPECT_EQ(final_result, "42");
}

TEST(AsyncChainDemo, MultilevelChain) {
    std::string final_result;
    bool final_ok = false;
    auto step1 = [](auto next, Result<std::string, std::string>) {
        next(Result<std::string, std::string>::Ok("OK"));
    };
    auto step2 = [](auto next, Result<std::string, std::string>) {
        auto s1 = [](auto next, Result<std::string, std::string>) {
            next(Result<std::string, std::string>::Ok("OK from internal s1"));
        };
        auto s2 = [](auto next, Result<std::string, std::string>) {
            next(Result<std::string, std::string>::Err("OK from internal s2"));
        };
        auto final = [&next](Result<std::string, std::string> result) {
            next(result);
        };
        initAsyncChain<std::string, std::string>()
            .then(s1)
            .then(s2)
            .finally(final);
    };
    auto finalStep = [&](Result<std::string, std::string> result) {
        if (result.is_ok()) {
            final_ok = true;
            final_result = *result.value;
        } else {
            final_ok = false;
            final_result = *result.error;
        }
    };
    setScheduler([](std::function<void()> task, std::size_t) { task(); });
    initAsyncChain<std::string, std::string>()
        .then(step1)
        .then(step2)
        .finally(finalStep);
    EXPECT_FALSE(final_ok);
    EXPECT_EQ(final_result, "OK from internal s2");
}

TEST(AsyncChainDemo, NestedChainWithErrorRecovery) {
    std::string final_result;
    bool final_ok = false;
    auto step1 = [](auto next, Result<std::string, std::string>) {
        next(Result<std::string, std::string>::Ok("outer1"));
    };
    auto step2 = [](auto next, Result<std::string, std::string> result) {
        // Launch a nested chain that fails
        auto nested1 = [](auto next, Result<std::string, std::string>) {
            next(Result<std::string, std::string>::Err("nested error"));
        };
        auto nested_final = [&next](Result<std::string, std::string> nested_result) {
            next(nested_result);
        };
        initAsyncChain<std::string, std::string>()
            .then(nested1)
            .finally(nested_final);
    };
    auto catcher = [](auto next, Result<std::string, std::string> error_result) {
        // Recover from nested error
        next(Result<std::string, std::string>::Ok("recovered from " + *error_result.error));
    };
    auto finalStep = [&](Result<std::string, std::string> result) {
        if (result.is_ok()) {
            final_ok = true;
            final_result = *result.value;
        } else {
            final_ok = false;
            final_result = *result.error;
        }
    };
    setScheduler([](std::function<void()> task, std::size_t) { task(); });
    initAsyncChain<std::string, std::string>()
        .then(step1)
        .then(step2)
        .catchError(catcher)
        .finally(finalStep);
    EXPECT_TRUE(final_ok);
    EXPECT_EQ(final_result, "recovered from nested error");
}

TEST(AsyncChainDemo, DeeplyNestedMultiLevelChain) {
    std::string final_result;
    bool final_ok = false;
    auto step1 = [](auto next, Result<std::string, std::string>) {
        // Launch a nested chain that itself launches another chain
        auto nested1 = [](auto next, Result<std::string, std::string>) {
            auto nested2 = [](auto next, Result<std::string, std::string>) {
                next(Result<std::string, std::string>::Ok("deep value"));
            };
            auto nested2_final = [&next](Result<std::string, std::string> result) {
                next(result);
            };
            initAsyncChain<std::string, std::string>()
                .then(nested2)
                .finally(nested2_final);
        };
        auto nested1_final = [&next](Result<std::string, std::string> result) {
            next(result);
        };
        initAsyncChain<std::string, std::string>()
            .then(nested1)
            .finally(nested1_final);
    };
    auto finalStep = [&](Result<std::string, std::string> result) {
        if (result.is_ok()) {
            final_ok = true;
            final_result = *result.value;
        } else {
            final_ok = false;
            final_result = *result.error;
        }
    };
    setScheduler([](std::function<void()> task, std::size_t) { task(); });
    initAsyncChain<std::string, std::string>()
        .then(step1)
        .finally(finalStep);
    EXPECT_TRUE(final_ok);
    EXPECT_EQ(final_result, "deep value");
}