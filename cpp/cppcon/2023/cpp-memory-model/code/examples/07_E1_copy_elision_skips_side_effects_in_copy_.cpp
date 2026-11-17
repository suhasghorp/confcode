#include <iostream>

struct noisy {
    noisy() { std::cout << "construct\n"; }
    noisy(const noisy&) { std::cout << "copy\n"; }
    noisy(noisy&&) noexcept { std::cout << "move\n"; }
    ~noisy() { std::cout << "destroy\n"; }
};

noisy make() { return noisy{}; }   // prvalue: guaranteed elision since C++17

int main()
{
    noisy n = make();              // prints "construct" once and "destroy" once; no copy, no move
}
