#include <cassert>
#include <cstddef>
#include <functional>
#include <limits>
#include <vector>

// A flat open-addressing set that needs two reserved key values.
template <typename T>
class sentinel_set {
public:
    sentinel_set(std::size_t capacity, T empty_key, T deleted_key)
        : slots_(capacity, empty_key), empty_(empty_key), deleted_(deleted_key) {}

    void insert(const T& v)
    {
        assert(v != empty_ && v != deleted_);        // users must never store these values!
        for (std::size_t i = std::hash<T>{}(v) % slots_.size();; i = (i + 1) % slots_.size()) {
            if (slots_[i] == v) return;
            if (slots_[i] == empty_ || slots_[i] == deleted_) { slots_[i] = v; return; }
        }
    }

    bool contains(const T& v) const
    {
        for (std::size_t i = std::hash<T>{}(v) % slots_.size();; i = (i + 1) % slots_.size()) {
            if (slots_[i] == empty_) return false;
            if (slots_[i] == v) return true;
        }
    }

private:
    std::vector<T> slots_;          // values stored inline: no nodes, no pointers
    T empty_, deleted_;
};

int main()
{
    constexpr int kEmpty = std::numeric_limits<int>::min();       // "impossible" values,
    constexpr int kDeleted = std::numeric_limits<int>::min() + 1; // chosen by the user
    sentinel_set<int> s(64, kEmpty, kDeleted);
    s.insert(42);
    return s.contains(42) ? 0 : 1;
}
