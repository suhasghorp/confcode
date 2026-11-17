#include <algorithm>
#include <chrono>
#include <cstdio>
#include <new>
#include <thread>
#include <vector>

void accumulate_fib(unsigned& result, unsigned n)
{
    if (n < 2) {
        result += n;
    } else {
        accumulate_fib(result, n - 1);
        accumulate_fib(result, n - 2);
    }
}

struct alignas(std::hardware_destructive_interference_size) PaddedSlot
{
    unsigned value = 0;
};

constexpr unsigned work = 4096;

template <typename Body>
double run(unsigned threads, Body body)
{
    auto start = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> pool;
        for (unsigned t = 0; t < threads; ++t)
            pool.emplace_back(body, t, threads);
    }   // jthreads join here
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int main()
{
    unsigned threads = std::max(2u, std::min(4u, std::thread::hardware_concurrency()));
    std::vector<unsigned> adjacent(threads);
    std::vector<PaddedSlot> padded(threads);
    std::vector<unsigned> results(threads);

    double t1 = run(threads, [&](unsigned id, unsigned n) {
        adjacent[id] = 0;
        for (unsigned i = 0; i != work / n; ++i) accumulate_fib(adjacent[id], i % 20);
    });
    double t2 = run(threads, [&](unsigned id, unsigned n) {
        padded[id].value = 0;
        for (unsigned i = 0; i != work / n; ++i) accumulate_fib(padded[id].value, i % 20);
    });
    double t3 = run(threads, [&](unsigned id, unsigned n) {
        unsigned local = 0;
        for (unsigned i = 0; i != work / n; ++i) accumulate_fib(local, i % 20);
        results[id] = local;
    });

    std::printf("%u threads, sizeof(PaddedSlot) = %zu\n", threads, sizeof(PaddedSlot));
    std::printf("adjacent slots: %7.1f ms (result %u)\n", t1, adjacent[0]);
    std::printf("padded slots:   %7.1f ms (result %u)\n", t2, padded[0].value);
    std::printf("local variable: %7.1f ms (result %u)\n", t3, results[0]);
}
