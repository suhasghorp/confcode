#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <set>
#include <unordered_set>
#include <vector>

volatile std::size_t sink;

template <typename Lookup>
double ns_per_lookup(const std::vector<int>& queries, Lookup lookup)
{
    auto start = std::chrono::steady_clock::now();
    std::size_t hits = 0;
    for (int rounds = 0; rounds < 200; ++rounds)
        for (int q : queries)
            hits += lookup(q);
    auto stop = std::chrono::steady_clock::now();
    sink = hits;
    return std::chrono::duration<double, std::nano>(stop - start).count()
           / (200.0 * queries.size());
}

int main()
{
    std::mt19937 engine(42);
    std::uniform_int_distribution<int> dist(0, 1 << 20);

    std::printf("%6s %10s %14s %10s %10s\n", "N", "set", "unordered_set", "unsorted", "sorted");
    for (int n : {64, 256, 1024}) {
        std::vector<int> values(n);
        for (int& v : values) v = dist(engine);

        std::set<int> tree(values.begin(), values.end());
        std::unordered_set<int> hash(values.begin(), values.end());
        std::vector<int> unsorted = values;
        std::vector<int> sorted = values;
        std::ranges::sort(sorted);

        // half the queries hit, half (almost certainly) miss
        std::vector<int> queries(10'000);
        for (std::size_t i = 0; i < queries.size(); ++i)
            queries[i] = i % 2 ? values[engine() % n] : dist(engine);

        std::printf("%6d %10.1f %14.1f %10.1f %10.1f\n", n,
            ns_per_lookup(queries, [&](int q) { return tree.contains(q); }),
            ns_per_lookup(queries, [&](int q) { return hash.contains(q); }),
            ns_per_lookup(queries, [&](int q) { return std::ranges::find(unsorted, q) != unsorted.end(); }),
            ns_per_lookup(queries, [&](int q) { return std::ranges::binary_search(sorted, q); }));
    }
}
