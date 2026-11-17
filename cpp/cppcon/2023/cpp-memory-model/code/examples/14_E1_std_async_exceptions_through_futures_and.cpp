#include <future>
#include <iostream>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

int main()
{
    // std::async: the promise/future plumbing is done for you.
    std::vector<int> numbers(500);
    std::iota(numbers.begin(), numbers.end(), 0);
    auto sum = std::async(std::launch::async, [&] { return std::accumulate(numbers.begin(), numbers.end(), 0); });
    std::cout << "sum = " << sum.get() << '\n';                    // 124750

    // Exceptions travel through the future.
    auto bad = std::async(std::launch::async, []() -> int { throw std::runtime_error("boom"); });
    try { bad.get(); } catch (const std::exception& e) { std::cout << "caught: " << e.what() << '\n'; }

    // Two mutexes, locked in opposite orders by two threads, without deadlock.
    std::mutex m1, m2;
    long a = 0, b = 0;
    {
        std::jthread t1([&] { for (int i = 0; i < 10000; ++i) { std::scoped_lock l(m1, m2); ++a; ++b; } });
        std::jthread t2([&] { for (int i = 0; i < 10000; ++i) { std::scoped_lock l(m2, m1); ++a; ++b; } });
    }
    std::cout << a << ' ' << b << '\n';                              // 20000 20000
}
