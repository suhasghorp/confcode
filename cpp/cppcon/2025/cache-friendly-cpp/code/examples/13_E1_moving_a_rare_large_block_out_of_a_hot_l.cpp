#include <cstdio>
#include <random>
#include <vector>

// A stand-in for benchmark::DoNotOptimize: the compiler must assume x is read.
#define KEEP(x) asm volatile("" : : "r"(x) : "memory")

#define REPEAT4(x) x; x; x; x
#define REPEAT16(x) REPEAT4(x); REPEAT4(x); REPEAT4(x); REPEAT4(x)
#define REPEAT64(x) REPEAT16(x); REPEAT16(x); REPEAT16(x); REPEAT16(x)

[[gnu::noinline, gnu::cold]] void rare_work(unsigned& count)
{
    REPEAT64(KEEP(++count));
}

unsigned sum_with_cold_path(const std::vector<unsigned>& data)
{
    unsigned count = 0;
    for (auto d : data) {
        count += d;
        if (d == 0) [[unlikely]]   // rarely true
            rare_work(count);
    }
    return count;
}

int main()
{
    std::vector<unsigned> data(1 << 20);
    std::mt19937 engine(9);
    for (auto& d : data) d = engine();
    data[123] = 0;   // make sure the rare path runs once
    std::printf("%u\n", sum_with_cold_path(data));
}
