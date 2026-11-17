#include <iostream>
#include <thread>

int main() {
    int counter = 0;
    {
        std::jthread a([&] { for (int i = 0; i < 1'000'000; ++i) ++counter; });
        std::jthread b([&] { for (int i = 0; i < 1'000'000; ++i) ++counter; });
    } // both jthreads join here

    // Data race on a non-atomic int: undefined behaviour.
    // In practice this frequently prints something less than 2000000.
    std::cout << counter << '\n';
}

