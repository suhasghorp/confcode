#include <array>
#include <cstddef>
#include <numeric>

template <std::size_t N>
constexpr std::array<int, N> get_int_array()
{
    std::array<int, N> a{};
    std::iota(a.begin(), a.end(), 0);
    return a;
}

constexpr std::size_t get_pivot(std::size_t n) { return n / 2; }   // an assumption: the talk doesn't show it

int main()
{
    constexpr std::size_t N = 100;
    auto data = get_int_array<N>();
    std::size_t p_sum{};
    for (std::size_t i{}; i < N; i++) {
        auto p = get_pivot(N);
        p_sum += data[i] * data[p];
    }
    return static_cast<int>(p_sum % 256);   // look at the -O2 assembly: no loop remains
}
// g++ -O2 -S -masm=intel: main is just "mov eax, <constant>; ret"  (247500 % 256 = 204)
