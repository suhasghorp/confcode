#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class spinlock {
    std::atomic<bool> locked_{false};
public:
    void lock()
    {
        while (locked_.exchange(true, std::memory_order_acquire)) {   // try to take it
            locked_.wait(true, std::memory_order_relaxed);            // sleep until it changes (C++20)
        }
    }
    void unlock()
    {
        locked_.store(false, std::memory_order_release);
        locked_.notify_one();
    }
};

int main()
{
    spinlock m;
    long counter = 0;
    {
        std::vector<std::jthread> ts;
        for (int t = 0; t < 4; ++t)
            ts.emplace_back([&] { for (int i = 0; i < 100'000; ++i) { std::lock_guard g(m); ++counter; } });
    }
    std::cout << counter << '\n';   // 400000
}
