#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

template <typename T, typename Hash = std::hash<T>>
class probing_node_set {
public:
    explicit probing_node_set(std::size_t capacity) : slots_(capacity) {}

    bool insert(const T& v)
    {
        for (std::size_t i = Hash{}(v) % slots_.size();; i = (i + 1) % slots_.size()) {
            if (!slots_[i]) { slots_[i] = std::make_unique<T>(v); return true; }   // free slot
            if (*slots_[i] == v) return false;                                     // already there
        }
    }

    const T* find(const T& v) const
    {
        for (std::size_t i = Hash{}(v) % slots_.size();; i = (i + 1) % slots_.size()) {
            if (!slots_[i]) return nullptr;          // an empty slot ends the probe sequence
            if (*slots_[i] == v) return slots_[i].get();
        }
    }

private:
    std::vector<std::unique_ptr<T>> slots_;          // nodes keep pointer stability
};

int main()
{
    probing_node_set<int> s(16);
    for (int v : {3, 19, 35, 7}) s.insert(v);       // 3, 19, 35 collide modulo 16
    const int* p = s.find(35);
    std::cout << (p ? *p : -1) << ' ' << (s.find(51) ? "found" : "missing") << '\n';
}
