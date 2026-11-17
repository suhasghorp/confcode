#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <vector>

constexpr std::uint64_t align8(std::uint64_t x) { return (x + 7) & ~std::uint64_t{7}; }

class Ring {                                         // power-of-two ring; copies split at the end
    std::vector<std::byte> buf_;
    std::uint64_t mask_;
public:
    std::atomic<std::uint64_t> read_counter{0};      // how far consumers may read (published)
    explicit Ring(std::size_t capacity) : buf_(capacity), mask_(capacity - 1) {}
    std::uint64_t capacity() const { return buf_.size(); }
    void copy_in(std::uint64_t pos, void const* src, std::size_t n) {
        auto off = pos & mask_, first = std::min<std::uint64_t>(n, buf_.size() - off);
        std::memcpy(buf_.data() + off, src, first);
        std::memcpy(buf_.data(), static_cast<std::byte const*>(src) + first, n - first);
    }
    void copy_out(void* dst, std::uint64_t pos, std::size_t n) const {
        auto off = pos & mask_, first = std::min<std::uint64_t>(n, buf_.size() - off);
        std::memcpy(dst, buf_.data() + off, first);
        std::memcpy(static_cast<std::byte*>(dst) + first, buf_.data(), n - first);
    }
};

class Producer {
    Ring& ring_;
    std::uint64_t local_counter_ = 0;
public:
    explicit Producer(Ring& r) : ring_(r) {}
    void write(std::span<std::byte const> data) {    // never waits for consumers
        auto pos = align8(local_counter_);
        auto size = std::uint32_t(data.size());
        ring_.copy_in(pos, &size, sizeof size);                     // 8-byte header slot
        ring_.copy_in(pos + sizeof(std::uint64_t), data.data(), size);
        local_counter_ = pos + sizeof(std::uint64_t) + size;
        ring_.read_counter.store(local_counter_, std::memory_order_release);   // finish_write
    }
};

constexpr std::int64_t no_data = 0, lapped = -1;

class Consumer {
    Ring const& ring_;
    std::uint64_t local_counter_ = 0, cached_counter_ = 0;
public:
    explicit Consumer(Ring const& r) : ring_(r) {}
    std::uint64_t behind() const { return ring_.read_counter.load(std::memory_order_acquire) - local_counter_; }
    bool is_lapped() const { return behind() > ring_.capacity(); }

    std::expected<std::size_t, std::int64_t> try_read(std::span<std::byte> data) {
        auto read_counter = align8(local_counter_);
        std::uint32_t size;
        if (read_counter + sizeof size >= cached_counter_) {
            cached_counter_ = ring_.read_counter.load(std::memory_order_acquire);
            if (read_counter + sizeof size > cached_counter_) return std::unexpected(no_data);
        }
        ring_.copy_out(&size, read_counter, sizeof size);
        if (size > data.size()) return std::unexpected(is_lapped() ? lapped : std::int64_t(size));
        read_counter += sizeof(std::uint64_t) + size;
        if (read_counter > cached_counter_) return std::unexpected(no_data);
        ring_.copy_out(data.data(), read_counter - size, size);
        if (is_lapped()) return std::unexpected(lapped);            // copied data may be overwritten
        local_counter_ = read_counter;
        return size;
    }
};

int main() {
    Ring ring(64);
    Producer producer(ring);
    Consumer fast(ring), slow(ring);
    std::byte buf[32];

    std::printf("fast consumer:");
    for (int i = 0; i < 10; ++i) {
        std::string msg = "m" + std::to_string(i);
        producer.write(std::as_bytes(std::span(msg)));
        if (auto n = fast.try_read(buf)) std::printf(" %.*s", int(*n), reinterpret_cast<char const*>(buf));
    }
    std::printf("\n");

    auto unread_overwritten = slow.behind() - ring.capacity();
    if (auto r = slow.try_read(buf); !r && r.error() == lapped)
        std::printf("slow consumer: lapped (the producer overwrote %llu unread bytes)\n",
                    static_cast<unsigned long long>(unread_overwritten));
}
