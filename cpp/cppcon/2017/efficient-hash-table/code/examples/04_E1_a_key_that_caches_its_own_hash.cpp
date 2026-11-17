#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_set>

class hashed_string {
public:
    explicit hashed_string(std::string s) : s_(std::move(s)), h_(std::hash<std::string>{}(s_)) {}
    const std::string& str() const noexcept { return s_; }
    std::size_t hash() const noexcept { return h_; }

    friend bool operator==(const hashed_string& a, const hashed_string& b)
    {
        return a.h_ == b.h_ && a.s_ == b.s_;     // cheap fast path, then the real comparison
    }

private:
    std::string s_;
    std::size_t h_;                              // computed once, reused on every rehash
};

struct cached_hash {
    std::size_t operator()(const hashed_string& k) const noexcept { return k.hash(); }
};

int main()
{
    std::unordered_set<hashed_string, cached_hash> s;
    for (int i = 0; i < 1000; ++i) s.emplace("a fairly long key number " + std::to_string(i));
    std::cout << s.count(hashed_string("a fairly long key number 42")) << '\n';   // 1
}
