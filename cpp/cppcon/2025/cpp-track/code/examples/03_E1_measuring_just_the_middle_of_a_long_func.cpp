#include <chrono>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

struct scope_timer {
    const char* name;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    ~scope_timer()
    {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
        std::cout << name << ": " << us.count() << " us\n";
    }
};

long long long_legacy_function()
{
    std::vector<int> data(1'000'000);                      // setup we don't care about
    std::iota(data.begin(), data.end(), 0);

    long long sum = 0;
    {
        scope_timer t{"core loop"};                        // measure only this block
        for (int x : data) sum += static_cast<long long>(x) * x % 7;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));   // "logging" we don't care about
    return sum;
}

int main() { std::cout << long_legacy_function() << '\n'; }
