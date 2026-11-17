#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <thread>

template <std::size_t N>              // N must be a power of two
class SpscRing {
    static_assert((N & (N - 1)) == 0);
    std::array<int, N> slots_{};
    std::atomic<std::size_t> head_{0};   // written only by the producer
    std::atomic<std::size_t> tail_{0};   // written only by the consumer
public:
    bool try_push(int v) {
        auto head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) == N) return false;  // full
        slots_[head & (N - 1)] = v;
        head_.store(head + 1, std::memory_order_release);   // publish the slot
        return true;
    }
    std::optional<int> try_pop() {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return std::nullopt; // empty
        int v = slots_[tail & (N - 1)];
        tail_.store(tail + 1, std::memory_order_release);   // free the slot
        return v;
    }
};

int main() {
    SpscRing<1024> q;
    std::jthread producer([&] {
        for (int i = 1; i <= 1'000'000; ++i)
            while (!q.try_push(i)) std::this_thread::yield();
    });
    long long sum = 0;
    for (int received = 0; received < 1'000'000;)
        if (auto v = q.try_pop()) { sum += *v; ++received; }
    std::printf("sum = %lld (expected 500000500000)\n", sum);
}
