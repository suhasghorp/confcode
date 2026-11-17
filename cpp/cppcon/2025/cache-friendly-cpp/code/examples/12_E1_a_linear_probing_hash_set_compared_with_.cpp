#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <unordered_set>
#include <vector>

class FlatIntSet
{
    std::vector<int> slots_;   // -1 = empty
    std::size_t mask_;

    std::size_t slot_for(int key) const
    {
        auto h = static_cast<std::uint64_t>(key) * 0x9E3779B97F4A7C15ull;   // Fibonacci hashing
        return static_cast<std::size_t>(h >> 32) & mask_;
    }

public:
    explicit FlatIntSet(std::size_t max_elements)
    {
        std::size_t capacity = 16;
        while (capacity < 2 * max_elements) capacity *= 2;   // keep load factor <= 0.5
        slots_.assign(capacity, -1);
        mask_ = capacity - 1;
    }

    void insert(int key)   // key must be >= 0
    {
        for (std::size_t i = slot_for(key);; i = (i + 1) & mask_) {
            if (slots_[i] == key) return;
            if (slots_[i] == -1) { slots_[i] = key; return; }
        }
    }

    bool contains(int key) const
    {
        for (std::size_t i = slot_for(key);; i = (i + 1) & mask_) {   // neighbours: same cache line
            if (slots_[i] == key) return true;
            if (slots_[i] == -1) return false;
        }
    }
};

template <typename Set>
double ns_per_lookup(const Set& set, const std::vector<int>& queries, std::size_t& hits)
{
    auto start = std::chrono::steady_clock::now();
    for (int q : queries) hits += set.contains(q);
    auto stop = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(stop - start).count() / queries.size();
}

int main()
{
    std::mt19937 engine(5);
    std::uniform_int_distribution<int> dist(0, 1 << 30);
    for (std::size_t n : {1'000, 100'000, 2'000'000}) {
        FlatIntSet flat(n);
        std::unordered_set<int> chained;
        std::vector<int> keys(n);
        for (int& k : keys) { k = dist(engine); flat.insert(k); chained.insert(k); }

        std::vector<int> queries(4'000'000);
        for (std::size_t i = 0; i < queries.size(); ++i)
            queries[i] = i % 2 ? keys[engine() % n] : dist(engine);

        std::size_t hits_flat = 0, hits_std = 0;
        double t_flat = ns_per_lookup(flat, queries, hits_flat);
        double t_std = ns_per_lookup(chained, queries, hits_std);
        std::printf("n=%8zu  open addressing %5.1f ns  unordered_set %5.1f ns  (hits %zu/%zu)\n",
                    n, t_flat, t_std, hits_flat, hits_std);
    }
}
