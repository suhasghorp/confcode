#include <algorithm>
#include <iostream>
#include <vector>

struct interval { double start, end; };

// Length of the union of intervals (they may overlap).
double union_length(std::vector<interval> v)
{
    std::sort(v.begin(), v.end(), [](auto a, auto b) { return a.start < b.start; });
    double total = 0, cur_s = 0, cur_e = -1;
    for (auto [s, e] : v) {
        if (s > cur_e) { total += std::max(0.0, cur_e - cur_s); cur_s = s; cur_e = e; }
        else cur_e = std::max(cur_e, e);
    }
    return total + std::max(0.0, cur_e - cur_s);
}

int main()
{
    interval fast{0, 1000};                       // fast_function: 0..1000 ms
    std::vector<interval> children{{100, 1000}};  // slow_function called after 100 ms of own work

    double inclusive = fast.end - fast.start;
    double exclusive = inclusive - union_length(children);
    std::cout << "fast_function inclusive " << inclusive << " ms, exclusive " << exclusive << " ms\n";  // 1000, 100
}
