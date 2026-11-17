#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

long long sum_big(const std::vector<int>& v)
{
    long long s = 0;
    for (int rep = 0; rep < 20; ++rep)
        for (int x : v)
            if (x >= 128) s += x;          // predictable only if the data is sorted
    return s;
}

int main()
{
    std::vector<int> v(1 << 20);
    std::mt19937 rng(1);
    for (auto& x : v) x = static_cast<int>(rng() % 256);

    auto time = [&](const char* label) {
        auto t0 = std::chrono::steady_clock::now();
        long long s = sum_big(v);
        auto t1 = std::chrono::steady_clock::now();
        std::cout << label << ": " << std::chrono::duration<double, std::milli>(t1 - t0).count()
                  << " ms (sum " << s << ")\n";
    };
    time("unsorted");
    std::sort(v.begin(), v.end());
    time("sorted  ");
}
