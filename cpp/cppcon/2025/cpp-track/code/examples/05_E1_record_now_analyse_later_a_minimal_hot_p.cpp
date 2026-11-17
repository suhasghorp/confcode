#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

struct record { std::int64_t ns; };
std::vector<record> g_records;            // single-threaded sketch

inline void hot_path_record(std::int64_t ns) { g_records.push_back({ns}); }   // cheap: append only

void cold_path_report()                   // expensive work happens later, once
{
    std::vector<std::int64_t> v;
    for (auto r : g_records) v.push_back(r.ns);
    std::sort(v.begin(), v.end());
    double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    std::cout << "calls " << v.size() << ", mean " << mean << " ns, median " << v[v.size() / 2] << " ns\n";
}

int main()
{
    g_records.reserve(100'000);
    for (int i = 0; i < 100'000; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        volatile int x = i * i; (void)x;
        auto t1 = std::chrono::steady_clock::now();
        hot_path_record(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }
    cold_path_report();
}
