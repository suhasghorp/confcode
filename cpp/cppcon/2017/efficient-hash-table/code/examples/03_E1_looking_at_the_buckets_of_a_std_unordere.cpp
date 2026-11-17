#include <iostream>
#include <unordered_set>

int main()
{
    std::unordered_set<int> s;
    s.max_load_factor(1.0f);
    for (int v : {10, 3, 17, 24, 31, 5, 12}) s.insert(v);

    std::cout << "size " << s.size() << ", buckets " << s.bucket_count()
              << ", load factor " << s.load_factor() << '\n';

    for (std::size_t b = 0; b < s.bucket_count(); ++b) {
        if (s.bucket_size(b) == 0) continue;
        std::cout << "bucket " << b << ":";
        for (auto it = s.begin(b); it != s.end(b); ++it) std::cout << ' ' << *it;   // the chain
        std::cout << '\n';
    }
}
