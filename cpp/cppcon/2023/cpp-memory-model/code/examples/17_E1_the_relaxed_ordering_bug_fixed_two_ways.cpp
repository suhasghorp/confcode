#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>

std::atomic<int> flag{0};
std::uint64_t shared_val{};

void IncsharedXtimeAquire()
{
    for (int count{0}; count < 10000000;) {
        int expected{};
        if (flag.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
            shared_val++;                                   // protected by the lock
            count++;
            flag.store(0, std::memory_order_release);
        }
    }
}

std::atomic<std::uint64_t> counter{0};
void IncAtomic() { for (int i = 0; i < 10000000; ++i) counter.fetch_add(1, std::memory_order_relaxed); }

int main()
{
    { std::jthread a(IncsharedXtimeAquire), b(IncsharedXtimeAquire); }
    { std::jthread a(IncAtomic), b(IncAtomic); }
    std::cout << shared_val << ' ' << counter << '\n';      // 20000000 20000000
}
