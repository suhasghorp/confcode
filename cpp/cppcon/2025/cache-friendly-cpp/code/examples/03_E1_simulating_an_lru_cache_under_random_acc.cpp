#include <cstdio>
#include <list>
#include <random>
#include <unordered_map>

class LruCache {
    std::size_t capacity_;
    std::list<std::size_t> order_;   // front = most recently used
    std::unordered_map<std::size_t, std::list<std::size_t>::iterator> where_;

public:
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    bool access(std::size_t address)   // returns true on a hit
    {
        if (auto it = where_.find(address); it != where_.end()) {
            order_.splice(order_.begin(), order_, it->second);   // move to front
            return true;
        }
        if (order_.size() == capacity_) {   // evict least recently used
            where_.erase(order_.back());
            order_.pop_back();
        }
        order_.push_front(address);
        where_[address] = order_.begin();
        return false;
    }
};

int main()
{
    constexpr std::size_t cache_slots = 512;
    std::mt19937 engine(1);
    for (std::size_t memory = 128; memory <= 16384; memory *= 2) {
        LruCache cache(cache_slots);
        std::uniform_int_distribution<std::size_t> dist(0, memory - 1);
        for (int i = 0; i < 100'000; ++i) cache.access(dist(engine));   // warm up
        int hits = 0;
        for (int i = 0; i < 1'000'000; ++i) hits += cache.access(dist(engine));
        std::printf("data %6zu slots: hit rate %5.1f%%\n", memory, hits / 10'000.0);
    }
}
