#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

template <typename F>
void measure(const char* name, F f)
{
    auto start = std::chrono::steady_clock::now();
    std::uint64_t sum = f();
    auto stop = std::chrono::steady_clock::now();
    std::printf("%-9s %8.1f ms  (sum %llu)\n", name,
                std::chrono::duration<double, std::milli>(stop - start).count(),
                static_cast<unsigned long long>(sum));
}

int main()
{
    std::vector<std::uint32_t> data(16 << 20);   // 64 MB
    std::iota(data.begin(), data.end(), 0u);

    std::vector<std::uint32_t> order(data.size());
    std::iota(order.begin(), order.end(), 0u);
    std::ranges::shuffle(order, std::mt19937(3));

    measure("forward", [&] {
        std::uint64_t s = 0;
        for (std::size_t i = 0; i < data.size(); ++i) s += data[i];
        return s;
    });
    measure("backward", [&] {
        std::uint64_t s = 0;
        for (std::size_t i = data.size(); i > 0; --i) s += data[i - 1];
        return s;
    });
    measure("random", [&] {
        std::uint64_t s = 0;
        for (std::uint32_t i : order) s += data[i];   // order itself is read sequentially
        return s;
    });
}
