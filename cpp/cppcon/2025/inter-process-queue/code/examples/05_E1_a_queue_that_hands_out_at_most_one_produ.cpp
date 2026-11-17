#include <atomic>
#include <cstdio>
#include <expected>
#include <utility>

enum class RoleError { already_taken };

class Queue {
    struct Metadata { std::atomic<bool> producer_taken{false}; };   // lives in shared memory later
    Metadata meta_;
public:
    class Producer {
        Metadata* meta_;
        friend Queue;
        explicit Producer(Metadata& m) : meta_(&m) {}
    public:
        Producer(Producer&& o) noexcept : meta_(std::exchange(o.meta_, nullptr)) {}
        Producer& operator=(Producer&&) = delete;
        ~Producer() { if (meta_) meta_->producer_taken.store(false, std::memory_order_release); }
        void write(int /*message*/) { /* only writing is possible */ }
    };

    std::expected<Producer, RoleError> make_producer() {
        bool expected = false;
        if (!meta_.producer_taken.compare_exchange_strong(expected, true, std::memory_order_acquire))
            return std::unexpected(RoleError::already_taken);   // the Highlander rule
        return Producer(meta_);
    }
};

int main() {
    Queue q;
    {
        auto p1 = q.make_producer();
        std::printf("first producer: %s\n", p1 ? "ok" : "refused");
        auto p2 = q.make_producer();
        std::printf("second producer: %s\n", p2 ? "ok" : "refused (role already taken)");
    }
    auto p3 = q.make_producer();
    std::printf("after the first is gone: %s\n", p3 ? "ok" : "refused");
}
