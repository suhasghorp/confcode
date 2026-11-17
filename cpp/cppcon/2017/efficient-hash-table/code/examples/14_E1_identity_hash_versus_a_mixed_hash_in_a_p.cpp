#include <cstdint>
#include <functional>
#include <iostream>
#include <set>

std::uint64_t mix(std::uint64_t h)
{
    h *= 0x9E3779B97F4A7C15ull;      // ~2^64 / golden ratio, odd
    return h ^ (h >> 32);            // fold the well-mixed high bits down
}

int main()
{
    constexpr std::uint64_t capacity = 1024;               // power of two
    std::set<std::uint64_t> identity_positions, mixed_positions;
    for (std::uint64_t i = 0; i < 1000; ++i) {
        std::uint64_t key = i << 10;
        identity_positions.insert(std::hash<std::uint64_t>{}(key) & (capacity - 1));
        mixed_positions.insert(mix(key) & (capacity - 1));
    }
    std::cout << "distinct positions, identity hash: " << identity_positions.size() << '\n'   // 1
              << "distinct positions, mixed hash:    " << mixed_positions.size() << '\n';     // 801
}
