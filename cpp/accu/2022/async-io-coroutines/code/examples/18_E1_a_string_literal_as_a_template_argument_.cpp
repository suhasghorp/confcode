#include <algorithm>
#include <cstddef>
#include <iostream>

template <std::size_t N>
struct str {
    constexpr str(const char (&p)[N]) noexcept { std::copy_n(p, N, cstr); }
    friend std::ostream& operator<<(std::ostream& os, const str& s) { return os << s.cstr; }
    char cstr[N];
};

template <typename T, str name>
struct tagged {
    T value;
    void trace(const char* what) const { std::cerr << name << ": " << what << '\n'; }
};

int main()
{
    tagged<int, "incoming"> a{4};
    tagged<int, "printer"> b{2};       // a different type from tagged<int, "incoming">
    a.trace("created");
    b.trace("created");
}
