// A single-writer sequence lock. The payload is stored as relaxed atomic
// words, so concurrent reads are not a data race in the C++ memory model.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <type_traits>

template <typename T>
class SeqLock {
  static_assert(std::is_trivially_copyable_v<T> && sizeof(T) % 8 == 0);
  static constexpr size_t kWords = sizeof(T) / 8;

 public:
  void Store(const T& value) {                                    // one writer only
    uint64_t v = seq_.load(std::memory_order_relaxed);
    seq_.store(v + 1, std::memory_order_relaxed);                 // odd: write in progress
    std::atomic_thread_fence(std::memory_order_release);          // odd seq before data
    std::array<uint64_t, kWords> w;
    std::memcpy(w.data(), &value, sizeof(T));
    for (size_t i = 0; i < kWords; ++i) words_[i].store(w[i], std::memory_order_relaxed);
    seq_.store(v + 2, std::memory_order_release);                 // even: data published
  }

  bool TryLoad(T& out) const {                                    // any number of readers
    uint64_t v1 = seq_.load(std::memory_order_acquire);
    if (v1 & 1) return false;                                     // writer active
    std::array<uint64_t, kWords> w;
    for (size_t i = 0; i < kWords; ++i) w[i] = words_[i].load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);          // data reads before re-check
    if (seq_.load(std::memory_order_relaxed) != v1) return false; // torn: retry
    std::memcpy(&out, w.data(), sizeof(T));
    return true;
  }

 private:
  std::atomic<uint64_t> seq_{0};
  std::array<std::atomic<uint64_t>, kWords> words_{};
};

struct Quote { uint64_t bid, ask, bid_size, ask_size; };  // 32 bytes

int main() {
  SeqLock<Quote> lock;
  std::atomic<bool> done{false};
  std::thread writer([&] {
    for (uint64_t i = 1; i <= 20'000'000; ++i) lock.Store({i, i, i, i});
    done.store(true);
  });
  uint64_t good = 0, torn = 0, retries = 0;
  while (!done.load()) {
    Quote q;
    if (!lock.TryLoad(q)) { ++retries; continue; }
    (q.bid == q.ask && q.ask == q.bid_size && q.bid_size == q.ask_size) ? ++good : ++torn;
  }
  writer.join();
  std::printf("consistent reads %llu, torn reads %llu, retries %llu\n",
              (unsigned long long)good, (unsigned long long)torn, (unsigned long long)retries);
}
