#include <atomic>
#include <iostream>
#include <thread>

int main()
{
    std::atomic<int> counter = 0;

    auto work = [&counter] {
        for (int i = 0; i < 1'000'000; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);   // indivisible
        }
    };

    {
        std::jthread t1(work);
        std::jthread t2(work);
    }

    std::cout << counter << '\n';   // always 2000000
}
