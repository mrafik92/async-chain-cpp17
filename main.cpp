
#include "async.hpp"
#include <iostream>
#include <string>
#include <functional>

struct MyService
{

    auto step1()
    {
        return [this](auto next)
        {
            std::cout << "Step1: always succeed\n";
            next(Result<std::string, std::string>::Ok("ok_from_step1"));
        };
    }

    auto step2()
    {
        return [this](auto next, int attempt)
        {
            std::cout << "Attempt " << attempt << "\n";
            if (attempt < 2)
            {
                next(Result<std::string, std::string>::Err("fail"));
            }
            else
            {
                next(Result<std::string, std::string>::Ok("42"));
            }
        };
    }

    void final(Result<std::string, std::string> result)
    {
        if (result.is_ok())
        {
            std::cout << "Final: success with value " << *result.value << "\n";
        }
        else
        {
            std::cout << "Final: failed with error " << *result.error << "\n";
        }
    }
};

void lamdas()
{
    std::cout << "Async Chain Example\n";
    using MyResult = Result<int, std::string>;

    auto step1 = [](auto next)
    {
        std::cout << "Step 1 OK\n";
        next(MyResult::Ok(123));
    };

    auto step2 = [](auto next)
    {
        std::cout << "Step 2 FAILS\n";
        next(Result<std::string, std::string>::Err("Step 2 error"));
    };

    auto catcher = [](auto next, auto error_result)
    {
        std::cout << "CatchError: recovered from " << *error_result.error << "\n";
        next(Result<std::string, std::string>::Ok("Recovered!"));
    };

    auto step3 = [](auto next)
    {
        std::cout << "Step 3 OK after recovery\n";
        next(Result<float, std::string>::Ok(3.14f));
    };

    auto finalStep = [](auto result)
    {
        if (result.is_ok())
        {
            std::cout << "Final Success: " << *result.value << "\n";
        }
        else
        {
            std::cout << "Final Failure: " << *result.error << "\n";
        }
    };

    int failure_count = 0;
    auto stepWithRetry = [&failure_count](auto next, int trial)
    {
        std::cout << "Step with retry attempt " << trial << "\n";

        if (failure_count < 2)
        { // simulate failure on first 2 attempts
            failure_count++;
            next(Result<std::string, std::string>::Err("Simulated failure"));
        }
        else
        {
            failure_count = 0;
            next(Result<std::string, std::string>::Ok("Success after retries"));
        }
    };



    callAllSmart<int, std::string>()
        .then<std::string>(step1)
        .then<std::string>(step2)
        .catchError(catcher)
        .then<float>(step3)
        .thenWithRetry<3, std::string>(stepWithRetry)              // 3 retries
        .thenWithRetryDelayed<3, 1000, std::string>(stepWithRetry) // 3 retries, 500ms delay each
        .finally(finalStep);
}

int main()
{
    MyService service;

    // Set fake scheduler (execute immediately, no real delay)
    setScheduler([](std::function<void()> task, std::size_t delay_ms)
                 {
        std::cout << "Scheduled with fake delay " << delay_ms << "ms\n";
        task(); });

    auto finalStep = [](auto result)
    {
        if (result.is_ok())
        {
            std::cout << "Final Success: " << *result.value << "\n";
        }
        else
        {
            std::cout << "Final Failure: " << *result.error << "\n";
        }
    };

    callAllSmart<std::string, std::string>()
        .then<std::string>(service.step1())
        .thenWithRetryDelayed<5, 1000, int>(service.step2()) // retry up to 5 times, 1000ms fake delay
        .finally(finalStep);
    
    lamdas();
    return 0;
}
