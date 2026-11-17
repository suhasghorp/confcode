// Requires Boost.Fiber (a library, not standard C++); link with boost_fiber and boost_context.
#include <boost/fiber/all.hpp>
#include <iostream>

// An ordinary function: not a coroutine, no special return type.
void step(const char* name, int i) {
    std::cout << name << i << '\n';
    boost::this_fiber::yield();  // suspends the WHOLE fiber, including its caller
}

void worker(const char* name) {
    for (int i = 0; i < 3; ++i)
        step(name, i);           // suspension happens one call level deeper
}

int main() {
    boost::fibers::fiber a(worker, "A");
    boost::fibers::fiber b(worker, "B");
    a.join();   // main's fiber waits; the scheduler runs a and b cooperatively
    b.join();
    // Output interleaves A and B because each yield hands control to the other fiber.
}
