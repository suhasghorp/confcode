#include <iostream>
#include <mutex>
#include <thread>

int main() {
    int counter = 0;
    std::mutex m;

    auto work = [&] {
        for (int i = 0; i < 1'000'000; ++i) {
            std::lock_guard lock(m); // lock on construction, unlock on destruction
            ++counter;               // only one thread can be here at a time
        }
    };

    {
        std::jthread a(work);
        std::jthread b(work);
    }

    std::cout << counter << '\n'; // always 2000000, but likely slower than one thread
}

