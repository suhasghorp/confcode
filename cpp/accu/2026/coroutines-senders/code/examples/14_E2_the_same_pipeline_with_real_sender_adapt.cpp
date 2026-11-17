#include <stdexec/execution.hpp>   // NVIDIA stdexec; in C++26: <execution>
#include <iostream>
#include <utility>

int main() {
    // before: lift the value 2 into the asynchronous world
    auto before = stdexec::just(2);

    // before | then(f) | after   -- builds the sender onion, runs nothing yet
    auto pipeline = std::move(before)
                  | stdexec::then([](int x) { return x * 2; })        // f
                  | stdexec::then([](int y) {                          // after
                        std::cout << "after: " << y << '\n';
                    });

    // sync_wait supplies the final receiver, connects (outside-in),
    // starts the operation, and blocks until completion flows back out.
    [[maybe_unused]] auto result = stdexec::sync_wait(std::move(pipeline));
}

