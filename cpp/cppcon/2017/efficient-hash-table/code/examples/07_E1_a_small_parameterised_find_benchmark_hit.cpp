#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

template <typename Set>
void run(const char* name, std::size_t n)
{
    std::mt19937_64 rng(n);
    std::vector<std::uint64_t> present(n), absent(n);
    for (auto& k : present) k = rng() | 1;          // odd keys are stored
    for (auto& k : absent) k = rng() & ~1ull;       // even keys never are

    Set s(present.begin(), present.end());          // setup: not timed
    std::shuffle(present.begin(), present.end(), rng);

    auto time = [&](const std::vector<std::uint64_t>& keys) {
        std::size_t found = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (auto k : keys) found += s.count(k);
        auto t1 = std::chrono::steady_clock::now();
        volatile std::size_t sink = found; (void)sink;            // use the result
        return std::chrono::duration<double, std::nano>(t1 - t0).count() / keys.size();
    };

    std::cout << name << " n=" << n << "  hit: " << time(present)
              << " ns  miss: " << time(absent) << " ns\n";
}

int main()
{
    for (std::size_t n : {100u, 10'000u, 1'000'000u})
        run<std::unordered_set<std::uint64_t>>("unordered_set", n);
}
