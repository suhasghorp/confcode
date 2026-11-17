#include <chrono>
#include <iostream>

int main()
{
    using clk = std::chrono::steady_clock;
    constexpr int N = 10'000'000;
    long long sink = 0;

    auto t0 = clk::now();
    for (int i = 0; i < N; ++i) sink += clk::now().time_since_epoch().count();   // the probe's floor cost
    auto t1 = clk::now();

    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
    std::cout << "steady_clock::now(): ~" << ns << " ns per call (sink " << (sink & 1) << ")\n";
}
