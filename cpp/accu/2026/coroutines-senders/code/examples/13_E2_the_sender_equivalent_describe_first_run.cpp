#include <execution>   // C++26; with NVIDIA stdexec use <stdexec/execution.hpp> and stdexec::
#include <iostream>

namespace ex = std::execution;

int main() {
    // A lazy description: "produce 1, then add 41". f has NOT been called yet.
    auto work = ex::just(1)
              | ex::then([](int x) {
                    std::cout << "running then\n";
                    return x + 41;
                });

    std::cout << "sender built, nothing ran\n";

    // sync_wait connects the sender to its own receiver, starts it, and blocks.
    // Returns std::optional<std::tuple<int>>; empty if the work was stopped.
    auto result = std::this_thread::sync_wait(std::move(work));
    auto [value] = result.value();
    std::cout << "value = " << value << '\n';   // value = 42
}
