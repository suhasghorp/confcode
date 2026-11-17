#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

int main()
{
    std::vector<double> ms;                                   // 90 fast calls, 10 slow ones
    for (int i = 0; i < 100; ++i) ms.push_back(i % 10 == 0 ? 100.0 : 1.0);
    std::sort(ms.begin(), ms.end());

    auto mean = [](auto b, auto e) { return std::accumulate(b, e, 0.0) / (e - b); };
    double m = mean(ms.begin(), ms.end());
    double sd = std::sqrt(std::accumulate(ms.begin(), ms.end(), 0.0,
                          [m](double acc, double x) { return acc + (x - m) * (x - m); }) / ms.size());

    std::size_t cut = ms.size() * 1 / 100;                    // 1% on each side
    auto cb = ms.begin() + cut, ce = ms.end() - cut;
    std::cout << "mean " << m << " ms, median " << ms[ms.size() / 2] << " ms, SD " << sd
              << " ms, CV " << sd / m << '\n';
    std::cout << "fastest[0-1]% " << ms.front() << " ms, center[1-99]% mean " << mean(cb, ce)
              << " ms, slowest[99-100]% " << ms.back() << " ms\n";
}
