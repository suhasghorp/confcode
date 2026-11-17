#include <iostream>
#include <stdexcept>
#include <thread>

void background_work() { /* ... */ }

void process(bool fail) {
    std::thread t(background_work);        // fork
    if (fail)
        throw std::runtime_error("oops");  // t is still joinable here...
    t.join();                              // ...so this join is skipped
}   // ~thread() on a joinable thread -> std::terminate()

int main() {
    try {
        process(true);
    } catch (const std::exception& e) {
        // Never reached: the program terminates during stack unwinding.
        std::cout << "caught: " << e.what() << '\n';
    }
}

