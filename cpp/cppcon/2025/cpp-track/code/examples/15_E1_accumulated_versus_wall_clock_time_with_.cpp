#include <algorithm>
#include <iostream>
#include <vector>

struct interval { double start, end; };

std::vector<interval> merge(std::vector<interval> v)
{
    std::sort(v.begin(), v.end(), [](const interval& a, const interval& b) { return a.start < b.start; });
    std::vector<interval> out;
    for (const auto& iv : v) {
        if (!out.empty() && out.back().end >= iv.start)
            out.back().end = std::max(out.back().end, iv.end);   // overlap: extend
        else
            out.push_back(iv);                                    // gap: new interval
    }
    return out;
}

int main()
{
    // Four threads, 10 ms each, starting 2 ms apart.
    std::vector<interval> events{{0, 10}, {2, 12}, {4, 14}, {6, 16}};
    double accumulated = 0, wall = 0;
    for (auto e : events) accumulated += e.end - e.start;
    for (auto m : merge(events)) wall += m.end - m.start;
    std::cout << "accumulated " << accumulated << " ms, wall-clock " << wall << " ms\n";   // 40, 16
}
