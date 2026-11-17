// C++26 <execution>. Before your standard library ships it, the same code
// works with NVIDIA's stdexec library (#include <stdexec/execution.hpp>,
// namespace stdexec, stdexec::sync_wait) or with beman.execution.
#include <execution>
#include <iostream>
#include <utility>

int main() {
    namespace ex = std::execution;

    // Describe the work; nothing runs yet.
    ex::sender auto work = ex::just(21)
                         | ex::then([](int x) { return x * 2; });

    // Launch it and block until it completes.
    // sync_wait returns std::optional<std::tuple<int>>.
    auto [result] = std::this_thread::sync_wait(std::move(work)).value();
    std::cout << result << '\n';  // prints 42
}

