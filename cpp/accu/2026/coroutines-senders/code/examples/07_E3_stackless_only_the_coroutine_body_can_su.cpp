#include <generator>   // C++23
#include <print>       // C++23
#include <ranges>

// To produce values from a helper, the helper must itself be a coroutine.
std::generator<int> inner(int base) {
    co_yield base;
    co_yield base + 1;
}

// Not allowed: an ordinary function cannot suspend its caller.
// void helper() { co_yield 42; }  // error: this would turn helper into a coroutine
//                                  // with an invalid return type

std::generator<int> outer() {
    co_yield 0;
    co_yield std::ranges::elements_of(inner(10));  // explicit hand-off to the nested coroutine
    co_yield 99;
}

int main() {
    for (int v : outer())
        std::println("{}", v);   // prints 0 10 11 99
}
