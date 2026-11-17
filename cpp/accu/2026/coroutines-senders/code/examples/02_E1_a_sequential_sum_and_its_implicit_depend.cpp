#include <iostream>
#include <vector>

int main() {
    std::vector<int> prices{3, 1, 4, 1, 5, 9, 2, 6};

    long total = 0;              // must happen before the loop
    for (int p : prices) {
        total += p;              // reads the result of the previous iteration
    }

    std::cout << total << '\n';  // assumes the whole loop is finished
}

