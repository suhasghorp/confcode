#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

using ctrl_t = signed char;
enum Ctrl : ctrl_t { kEmpty = -128, kDeleted = -2, kSentinel = -1 };
std::size_t H1(std::size_t hash) { return hash >> 7; }
ctrl_t H2(std::size_t hash) { return hash & 0x7F; }

std::size_t mix(std::size_t h) { return h * 0x9E3779B97F4A7C15ull; }  // spread bits (see "Defaults matter")

template <typename K>
class scalar_table {
public:
    explicit scalar_table(std::size_t n) : size_(n), ctrl_(n, kEmpty), slots_(n) {}

    std::optional<std::size_t> find(const K& key, std::size_t hash) const {
        std::size_t pos = H1(hash) % size_;
        while (true) {
            if (H2(hash) == ctrl_[pos] && key == slots_[pos])
                return pos;
            if (ctrl_[pos] == kEmpty) return std::nullopt;
            pos = (pos + 1) % size_;
        }
    }
    std::optional<std::size_t> find(const K& key) const { return find(key, mix(std::hash<K>{}(key))); }

    void insert(const K& key) {                       // not on the slides
        std::size_t hash = mix(std::hash<K>{}(key));
        if (find(key, hash)) return;
        for (std::size_t pos = H1(hash) % size_;; pos = (pos + 1) % size_)
            if (ctrl_[pos] < 0) { ctrl_[pos] = H2(hash); slots_[pos] = key; return; }
    }

private:
    std::size_t size_;               // number of positions (the slide's size_)
    std::vector<ctrl_t> ctrl_;       // one control byte per position
    std::vector<K> slots_;           // values stored inline
};

int main()
{
    scalar_table<int> t(64);
    for (int i = 0; i < 40; ++i) t.insert(i * 3);
    std::cout << t.find(27).has_value() << t.find(28).has_value() << '\n';   // 10
}
