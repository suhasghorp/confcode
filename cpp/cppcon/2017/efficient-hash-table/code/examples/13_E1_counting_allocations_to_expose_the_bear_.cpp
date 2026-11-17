#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>

static long g_allocs = 0;
void* operator new(std::size_t n) { ++g_allocs; if (void* p = std::malloc(n)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

template <typename F> long allocs(F f) { long before = g_allocs; f(); return g_allocs - before; }

// C++20 heterogeneous lookup: transparent hash + transparent equality.
struct string_hash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};

int main()
{
    const char key[] = "abcdefghijklmnopqrst";       // longer than the small-string buffer

    std::unordered_map<std::string, int> m{{key, 1}};
    std::printf("find(const char*): %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) (void)m.find(key); }));

    std::unordered_map<std::string, int, string_hash, std::equal_to<>> h{{key, 1}};
    std::printf("transparent find : %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) (void)h.find(std::string_view(key)); }));

    std::pair<const std::string, int> p{key, 0};
    std::printf("insert(pair&)    : %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) m.insert(p); }));
    std::string k = key;
    std::printf("emplace(k, 0)    : %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) m.emplace(k, 0); }));
    std::printf("try_emplace(k, 0): %ld\n", allocs([&] { for (int i = 0; i < 100; ++i) m.try_emplace(k, 0); }));
}
