#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

int main()
{
    std::unordered_set<int> s{3, 1, 4, 1, 5, 9, 2, 6};
    s.insert(7);
    std::cout << "contains 4? " << s.contains(4) << '\n';   // C++20
    s.erase(4);

    std::unordered_map<std::string, int> ages{{"ada", 36}, {"alan", 41}};
    ages["grace"] = 85;
    if (auto it = ages.find("alan"); it != ages.end()) std::cout << it->second << '\n';

    // The bucket interface: rarely used, but it constrains the implementation.
    std::cout << "buckets: " << s.bucket_count()
              << ", load factor: " << s.load_factor()
              << ", max load factor: " << s.max_load_factor()
              << ", bucket of 9: " << s.bucket(9) << '\n';
}
