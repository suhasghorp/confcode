#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

struct packed  { std::atomic<long> a{0}; std::atomic<long> b{0}; };                 // same cache line
struct padded  { alignas(64) std::atomic<long> a{0}; alignas(64) std::atomic<long> b{0}; };  // separate lines

template <typename Counters>
double run()
{
    Counters c;
    auto t0 = std::chrono::steady_clock::now();
    std::thread t1([&] { for (int i = 0; i < 20'000'000; ++i) c.a.fetch_add(1, std::memory_order_relaxed); });
    std::thread t2([&] { for (int i = 0; i < 20'000'000; ++i) c.b.fetch_add(1, std::memory_order_relaxed); });
    t1.join(); t2.join();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

int main()
{
    std::cout << "same line:      " << run<packed>() << " ms\n";
    std::cout << "separate lines: " << run<padded>() << " ms\n";
}
