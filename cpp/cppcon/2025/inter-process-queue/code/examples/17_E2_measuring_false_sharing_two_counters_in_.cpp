#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

constexpr std::size_t cache_line = 64;            // defined explicitly, as the speaker recommends

struct Adjacent { std::atomic<long> a{0}; std::atomic<long> b{0}; };
struct Separate { alignas(cache_line) std::atomic<long> a{0}; alignas(cache_line) std::atomic<long> b{0}; };

template <class Counters>
long long run_ms() {
    Counters c;
    auto start = std::chrono::steady_clock::now();
    {
        std::jthread t1([&] { for (int i = 0; i < 50'000'000; ++i) c.a.fetch_add(1, std::memory_order_relaxed); });
        std::jthread t2([&] { for (int i = 0; i < 50'000'000; ++i) c.b.fetch_add(1, std::memory_order_relaxed); });
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    return ms.count();
}

int main() {
    std::printf("same cache line:     %lld ms\n", run_ms<Adjacent>());
    std::printf("separate cache lines: %lld ms\n", run_ms<Separate>());
}
