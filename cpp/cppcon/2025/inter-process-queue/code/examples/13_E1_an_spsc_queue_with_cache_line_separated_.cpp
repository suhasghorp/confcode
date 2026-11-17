#include <array>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <thread>

constexpr std::size_t line = 64;

template <std::size_t N>
class Ring {
    static_assert((N & (N - 1)) == 0);
    struct alignas(line) Shared { std::atomic<std::size_t> pos{0}; };
    struct alignas(line) Local  { std::size_t pos = 0; std::size_t cached_other = 0; std::size_t refreshes = 0; };
    Shared writer_shared_, reader_shared_;      // each written by one side, read by the other
    Local writer_, reader_;                     // touched by one side only
    std::array<int, N> slots_{};
public:
    bool try_push(int v) {
        if (writer_.pos - writer_.cached_other == N) {               // looks full: refresh
            writer_.cached_other = reader_shared_.pos.load(std::memory_order_acquire);
            ++writer_.refreshes;
            if (writer_.pos - writer_.cached_other == N) return false;
        }
        slots_[writer_.pos++ & (N - 1)] = v;
        writer_shared_.pos.store(writer_.pos, std::memory_order_release);
        return true;
    }
    bool try_pop(int& out) {
        if (reader_.pos == reader_.cached_other) {                   // looks empty: refresh
            reader_.cached_other = writer_shared_.pos.load(std::memory_order_acquire);
            ++reader_.refreshes;
            if (reader_.pos == reader_.cached_other) return false;
        }
        out = slots_[reader_.pos++ & (N - 1)];
        reader_shared_.pos.store(reader_.pos, std::memory_order_release);
        return true;
    }
    std::size_t writer_refreshes() const { return writer_.refreshes; }
    std::size_t reader_refreshes() const { return reader_.refreshes; }
};

int main() {
    constexpr int count = 1'000'000;
    static Ring<4096> q;
    std::jthread producer([] {
        for (int i = 1; i <= count; ++i) while (!q.try_push(i)) std::this_thread::yield();
    });
    long long sum = 0;
    for (int got = 0, v; got < count;) if (q.try_pop(v)) { sum += v; ++got; }
    producer.join();
    std::printf("sum = %lld; writer refreshed %zu times, reader %zu times, for %d messages\n",
                sum, q.writer_refreshes(), q.reader_refreshes(), count);
}
