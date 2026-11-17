#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>

struct Payload { std::uint64_t x, y; };      // invariant: y == 2 * x

class SeqLocked {
    std::atomic<std::uint64_t> seq_{0};
    Payload data_{};                            // accessed only through atomic_ref
public:
    void write(Payload p) {                     // single writer
        auto s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);          // odd: write in progress
        std::atomic_thread_fence(std::memory_order_release);
        std::atomic_ref(data_.x).store(p.x, std::memory_order_relaxed);
        std::atomic_ref(data_.y).store(p.y, std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);          // even: stable
    }
    bool try_read(Payload& out) {
        auto s1 = seq_.load(std::memory_order_acquire);
        if (s1 & 1) return false;
        out.x = std::atomic_ref(data_.x).load(std::memory_order_relaxed);
        out.y = std::atomic_ref(data_.y).load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        return seq_.load(std::memory_order_relaxed) == s1;     // unchanged: the copy is consistent
    }
};

int main() {
    SeqLocked s;
    std::atomic<bool> done{false};
    std::jthread writer([&] {
        for (std::uint64_t i = 1; !done.load(std::memory_order_relaxed); ++i) s.write({i, 2 * i});
    });
    long reads = 0, retries = 0, torn = 0;
    while (reads < 1'000'000) {
        Payload p;
        if (!s.try_read(p)) { ++retries; continue; }
        ++reads;
        if (p.y != 2 * p.x) ++torn;
    }
    done = true;
    std::printf("reads: %ld, retries: %ld, torn reads: %ld\n", reads, retries, torn);
}
