#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

template <typename Age>
Age average_age(const std::vector<Age>& ages)
{
    int total = 0;
    for (Age age : ages) {
        total += static_cast<int>(age);
    }
    return static_cast<Age>(total / ages.size());
}

template <typename Underlying>
void run(const char* name)
{
    enum class Age : Underlying {};
    constexpr std::size_t bytes = 16 << 20;
    std::vector<Age> ages(bytes / sizeof(Age));
    std::mt19937 engine(7);
    std::uniform_int_distribution<int> dist(0, 100);
    for (Age& a : ages) a = static_cast<Age>(dist(engine));

    double best = 1e9;
    int result = 0;
    for (int rep = 0; rep < 5; ++rep) {
        auto start = std::chrono::steady_clock::now();
        result = static_cast<int>(average_age(ages));
        auto stop = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double>(stop - start).count());
    }
    std::printf("%-8s sizeof=%zu  %6.2f billion items/s  (avg %d)\n",
                name, sizeof(Age), ages.size() / best / 1e9, result);
}

int main()
{
    run<int>("int");
    run<short>("short");
    run<std::uint8_t>("uint8_t");
}
