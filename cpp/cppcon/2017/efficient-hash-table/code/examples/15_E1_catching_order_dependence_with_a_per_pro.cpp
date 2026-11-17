#include <cstdint>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

// A hasher that mixes in a seed chosen once per process (like the talk's debug-mode kSeed).
struct seeded_hash {
    static std::uint64_t seed()
    {
        static const std::uint64_t s = std::random_device{}();
        return s;
    }
    std::size_t operator()(int v) const
    {
        std::uint64_t h = (static_cast<std::uint64_t>(v) ^ seed()) * 0x9E3779B97F4A7C15ull;
        return static_cast<std::size_t>(h ^ (h >> 32));
    }
};

int main()
{
    std::unordered_set<int, seeded_hash> s{1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<int> order(s.begin(), s.end());

    // Code like this "works" with a deterministic hash and silently depends on it.
    // With a seeded hash its output changes from run to run, exposing the bug in testing.
    std::cout << "first element this run: " << order.front() << '\n';
}
