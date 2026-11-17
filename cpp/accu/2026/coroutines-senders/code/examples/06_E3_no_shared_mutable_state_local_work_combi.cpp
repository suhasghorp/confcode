#include <array>
#include <functional>
#include <iostream>
#include <thread>

int main() {
    std::array<long, 2> partial{};

    auto work = [](long& out) {
        long local = 0;                     // private to this thread
        for (int i = 0; i < 1'000'000; ++i) ++local;
        out = local;                        // single write to this thread's own slot
    };

    {
        std::jthread a(work, std::ref(partial[0]));
        std::jthread b(work, std::ref(partial[1]));
    } // the join here is the only synchronization we need

    std::cout << partial[0] + partial[1] << '\n'; // always 2000000
}

