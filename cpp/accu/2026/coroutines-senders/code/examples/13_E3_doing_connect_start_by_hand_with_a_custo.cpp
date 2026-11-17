#include <execution>   // C++26 std::execution
#include <exception>
#include <cstdio>
#include <utility>

namespace ex = std::execution;

// A receiver: the three completion channels of the protocol.
struct print_receiver {
    using receiver_concept = ex::receiver_t;

    void set_value(int v) && noexcept { std::printf("value: %d\n", v); }
    void set_error(std::exception_ptr) && noexcept { std::puts("error"); }
    void set_stopped() && noexcept { std::puts("stopped"); }
};

int main() {
    auto snd = ex::just(20) | ex::then([](int x) { return x + 22; });
    // snd is only a description; it is movable and nothing has run.

    auto op = ex::connect(std::move(snd), print_receiver{});
    // op is the operation state: a statically-typed nested struct living on the
    // stack. It is immovable, because a running operation may be pointed into.

    ex::start(op);   // now the work runs (inline here) and prints "value: 42"
}
