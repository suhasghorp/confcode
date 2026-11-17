#include <emmintrin.h>
#include <bit>
#include <cstdint>
#include <cstdio>

template <typename T>
struct BitMask {
    T mask;
    explicit BitMask(T m) : mask(m) {}
    struct iterator {
        T m;
        int operator*() const { return std::countr_zero(m); }   // index of lowest set bit
        iterator& operator++() { m &= m - 1; return *this; }     // clear lowest set bit
        bool operator!=(const iterator& o) const { return m != o.m; }
    };
    iterator begin() const { return {mask}; }
    iterator end() const { return {0}; }
};

BitMask<std::uint32_t> Match(__m128i ctrl, std::uint8_t h2)
{
    auto match = _mm_set1_epi8(static_cast<char>(h2));                  // 96 96 96 ... 96
    return BitMask<std::uint32_t>(
        _mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl)));               // FF where equal -> bits
}

int main()
{
    // The control bytes from the slide: 7F DF 96 32 F1 F8 EB 43, twice.
    alignas(16) unsigned char bytes[16] = {0x7F, 0xDF, 0x96, 0x32, 0xF1, 0xF8, 0xEB, 0x43,
                                           0x7F, 0xDF, 0x96, 0x32, 0xF1, 0xF8, 0xEB, 0x43};
    __m128i ctrl = _mm_load_si128(reinterpret_cast<const __m128i*>(bytes));
    for (int i : Match(ctrl, 0x96)) std::printf("candidate at %d\n", i);   // 2 and 10
}
