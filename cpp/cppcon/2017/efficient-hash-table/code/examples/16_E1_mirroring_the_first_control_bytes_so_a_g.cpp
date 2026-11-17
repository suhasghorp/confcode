#include <cstddef>
#include <cstdio>
#include <vector>

using ctrl_t = signed char;
constexpr ctrl_t kEmpty = -128;
constexpr std::size_t kGroupWidth = 16;

struct control_bytes {
    explicit control_bytes(std::size_t capacity)
        : capacity_(capacity), bytes_(capacity + kGroupWidth, kEmpty) {}

    // Every write to one of the first 16 bytes is mirrored past the end, so that a
    // 16-byte load starting at any position i < capacity reads valid, wrapped-around data.
    void set(std::size_t i, ctrl_t v)
    {
        bytes_[i] = v;
        if (i < kGroupWidth) bytes_[capacity_ + i] = v;     // the second write
    }

    const ctrl_t* group_at(std::size_t i) const { return bytes_.data() + i; }   // any offset

    std::size_t capacity_;
    std::vector<ctrl_t> bytes_;
};

int main()
{
    control_bytes c(64);
    c.set(3, 0x2A);                                   // also appears at index 64 + 3
    const ctrl_t* g = c.group_at(60);                 // a group spanning the wrap-around
    std::printf("byte 7 of the group starting at 60 (= position 3): %d\n", g[7]);   // 42
}
