#include <iostream>
#include <ranges>
#include <vector>

int main() {
    std::vector<int> v{1, 2, 3, 4, 5, 6};

    // Builds a nested view object (an expression template). Nothing is computed yet.
    auto pipeline = v
        | std::views::filter([](int x) { return x % 2 == 0; })
        | std::views::transform([](int x) { return x * 10; });

    // Work happens only now, element by element, as the loop pulls values.
    for (int x : pipeline) {
        std::cout << x << ' ';   // prints: 20 40 60
    }
    std::cout << '\n';
}
