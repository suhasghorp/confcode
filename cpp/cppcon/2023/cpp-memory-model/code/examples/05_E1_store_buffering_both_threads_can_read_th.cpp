#include <atomic>
#include <iostream>
#include <thread>

std::atomic<int> x{0}, y{0};
int r1, r2;

int main()
{
    int both_zero = 0;
    for (int i = 0; i < 200'000; ++i) {
        x = 0; y = 0;
        std::thread a([] { x.store(1, std::memory_order_relaxed); r1 = y.load(std::memory_order_relaxed); });
        std::thread b([] { y.store(1, std::memory_order_relaxed); r2 = x.load(std::memory_order_relaxed); });
        a.join(); b.join();
        both_zero += (r1 == 0 && r2 == 0);
    }
    // Thread creation overhead makes the window small, so the count is often low, but nonzero is allowed.
    std::cout << "r1 == r2 == 0 happened " << both_zero << " times\n";
}
