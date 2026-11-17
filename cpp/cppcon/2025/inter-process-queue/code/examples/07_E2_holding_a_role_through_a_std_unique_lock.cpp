#include <atomic>
#include <cstdio>
#include <mutex>
#include <optional>
#include <utility>

struct SpinLock {                                   // any type with lock/try_lock/unlock works
    std::atomic<bool> held{false};
    bool try_lock() { return !held.exchange(true, std::memory_order_acquire); }
    void lock() { while (!try_lock()) {} }
    void unlock() { held.store(false, std::memory_order_release); }
};

struct MetaData {
    SpinLock producer_lock_;
    std::unique_lock<SpinLock> try_producer_lock() { return {producer_lock_, std::try_to_lock}; }
};

class Producer {
    std::unique_lock<SpinLock> role_;               // owning the lock == being the producer
public:
    explicit Producer(std::unique_lock<SpinLock> role) : role_(std::move(role)) {}
};

std::optional<Producer> make_producer(MetaData& md) {
    auto lk = md.try_producer_lock();
    if (!lk.owns_lock()) return std::nullopt;
    return Producer(std::move(lk));
}

int main() {
    MetaData md;
    auto a = make_producer(md);
    auto b = make_producer(md);
    std::printf("a: %d, b: %d\n", a.has_value(), b.has_value());     // a: 1, b: 0
    a.reset();                                                        // role released by RAII
    std::printf("after a is gone: %d\n", make_producer(md).has_value());
}
