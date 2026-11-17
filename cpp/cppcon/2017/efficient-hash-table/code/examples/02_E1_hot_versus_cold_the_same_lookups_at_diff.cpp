#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

// Average nanoseconds per successful find, probing keys in random order.
double ns_per_find(std::size_t n)
{
    std::unordered_set<std::uint64_t> s;
    std::vector<std::uint64_t> keys(n);
    std::mt19937_64 rng(42);
    for (auto& k : keys) { k = rng(); s.insert(k); }
    std::shuffle(keys.begin(), keys.end(), rng);

    std::size_t hits = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 4; ++rep)
        for (auto k : keys) hits += s.count(k);
    auto t1 = std::chrono::steady_clock::now();
    if (hits == 0) std::cout << "";                       // keep the loop alive
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / (4.0 * n);
}

int main()
{
    for (std::size_t n : {1'000u, 100'000u, 4'000'000u})   // L1/L2-sized ... RAM-sized
        std::cout << n << " elements: " << ns_per_find(n) << " ns/find\n";
}
