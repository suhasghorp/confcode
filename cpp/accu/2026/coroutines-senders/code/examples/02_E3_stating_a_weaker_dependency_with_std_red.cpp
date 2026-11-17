#include <execution>  // C++17 execution policies
#include <iostream>
#include <numeric>
#include <vector>

int main() {
    std::vector<int> prices{3, 1, 4, 1, 5, 9, 2, 6};

    // std::reduce may add elements in any order and grouping,
    // and std::execution::par allows it to split the work across threads.
    long total = std::reduce(std::execution::par,
                             prices.begin(), prices.end(), 0L);

    std::cout << total << '\n';  // same result as the sequential loop: 31
}

