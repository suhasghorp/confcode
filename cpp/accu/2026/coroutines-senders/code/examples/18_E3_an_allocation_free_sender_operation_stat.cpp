// Uses NVIDIA stdexec (namespace stdexec::), the reference implementation
// of C++26 std::execution. With a C++26 standard library, replace
// stdexec:: with std::execution::.
#include <stdexec/execution.hpp>
#include <cstdio>
#include <exception>

struct print_receiver {
    using receiver_concept = stdexec::receiver_t;

    void set_value(int v) noexcept { std::printf("value: %d\n", v); }
    void set_error(std::exception_ptr) noexcept { std::printf("error\n"); }
    void set_stopped() noexcept { std::printf("stopped\n"); }
};

int main() {
    auto work = stdexec::just(20)
              | stdexec::then([](int x) { return x + 1; })
              | stdexec::then([](int x) { return x * 2; });

    // connect() returns the whole nested "onion" as one concrete object.
    // It lives on the stack: no heap allocation is needed to run the chain.
    auto op = stdexec::connect(std::move(work), print_receiver{});

    // Operation states are immovable: 'op' must stay put until completion.
    stdexec::start(op);   // just/then complete inline here, printing "value: 42"
}

