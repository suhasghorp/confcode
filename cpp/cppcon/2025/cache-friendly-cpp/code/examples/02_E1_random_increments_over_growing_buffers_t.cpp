#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

int main()
{
    constexpr std::size_t accesses = 20'000'000;
    for (std::size_t kb = 16; kb <= 64 * 1024; kb *= 2) {
        std::vector<unsigned> data(kb * 1024 / sizeof(unsigned), 1);
        std::uint64_t x = 88172645463325252ull;   // LCG state
        auto next_index = [&] {
            x = x * 6364136223846793005ull + 1442695040888963407ull;
            return (x >> 33) % data.size();
        };

        auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < accesses; ++i)
            ++data[next_index()];
        auto stop = std::chrono::steady_clock::now();

        unsigned check = 0;
        for (unsigned d : data) check += d;   // use the data so the loop is kept
        double ns = std::chrono::duration<double, std::nano>(stop - start).count() / accesses;
        std::printf("%8zu KB  %6.2f ns/access  (check %u)\n", kb, ns, check);
    }
}
