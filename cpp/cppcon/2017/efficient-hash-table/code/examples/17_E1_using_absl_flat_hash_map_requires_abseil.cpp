#include <string>
#include <string_view>
#include "absl/container/flat_hash_map.h"

int main()
{
    absl::flat_hash_map<std::string, int> ages;
    ages["ada"] = 36;
    ages.try_emplace("alan", 41);

    // Heterogeneous lookup works out of the box for string keys:
    std::string_view who = "ada";
    if (auto it = ages.find(who); it != ages.end()) return it->second == 36 ? 0 : 1;
    return 1;
}
