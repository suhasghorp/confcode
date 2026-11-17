#include <iostream>
#include <thread>

int main()
{
    int progress = 0;

    std::jthread reporter([&progress] {
        // Reads `progress` while main() writes it: a data race.
        for (int i = 0; i < 5; ++i) {
            std::cout << "progress: " << progress << '\n';
        }
    });

    for (int i = 0; i < 100; ++i) {
        ++progress;   // unsynchronised write
    }
}   // compiles cleanly; behaviour is undefined
