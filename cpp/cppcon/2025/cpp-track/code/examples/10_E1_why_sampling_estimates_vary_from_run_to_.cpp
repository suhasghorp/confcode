#include <algorithm>
#include <iostream>
#include <random>

// A 2-second timeline: [0,1) is function A, [1,2) is function B (truly 50/50).
// A profiler takes `samples` random snapshots and estimates A's share.
int main()
{
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> when(0.0, 2.0);
    for (int samples : {20, 100, 1000}) {
        double lo = 1, hi = 0;
        for (int run = 0; run < 50; ++run) {
            int a = 0;
            for (int i = 0; i < samples; ++i) a += when(rng) < 1.0;
            double share = double(a) / samples;
            lo = std::min(lo, share); hi = std::max(hi, share);
        }
        std::cout << samples << " samples/run: A's estimated share ranged " << lo * 100 << "% .. " << hi * 100 << "%\n";
    }
}
