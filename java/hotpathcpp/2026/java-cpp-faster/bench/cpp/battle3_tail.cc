// Battle 3 (C++ side): 2^21 resting orders, 50,000 bursts of 256
// replacements through a pre-faulted slab pool. Prints p50/p99/p99.99/max
// of the per-burst latency in microseconds.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <span>
#include <vector>

struct Order {
  int64_t order_id;
  int64_t price;
  int32_t quantity;
  int32_t symbol_id;
  int32_t flags;
};
static_assert(sizeof(Order) == 32);

// Fixed-capacity pool with an intrusive freelist. All memory is touched
// once at construction, so no page faults happen on the hot path.
template <typename T>
class SlabPool {
 public:
  explicit SlabPool(size_t capacity)
      : storage_(new Slot[capacity]), capacity_(capacity) {
    std::memset(static_cast<void*>(storage_), 0, capacity * sizeof(Slot));
    for (size_t i = 0; i + 1 < capacity; ++i) storage_[i].next = &storage_[i + 1];
    storage_[capacity - 1].next = nullptr;
    free_ = &storage_[0];
  }
  ~SlabPool() { delete[] storage_; }
  SlabPool(const SlabPool&) = delete;
  SlabPool& operator=(const SlabPool&) = delete;

  T* Allocate(int64_t id, int64_t px, int32_t qty) {
    Slot* s = free_;
    free_ = s->next;
    return ::new (static_cast<void*>(s->bytes)) T{id, px, qty, 1, 0};
  }
  void Free(T* p) {
    if (p == nullptr) return;
    Slot* s = reinterpret_cast<Slot*>(p);
    s->next = free_;
    free_ = s;
  }

 private:
  union Slot {
    Slot* next;
    alignas(T) unsigned char bytes[sizeof(T)];
  };
  Slot* storage_;
  size_t capacity_;
  Slot* free_ = nullptr;
};

constexpr uint32_t kBookSlots = 1u << 21;
constexpr uint32_t kBookMask = kBookSlots - 1;

void ApplyMutationBurst(SlabPool<Order>& pool, std::span<Order*> book,
                        uint32_t base_slot) {
  for (uint32_t j = 0; j < 256; ++j) {
    uint32_t idx = (base_slot + j) & kBookMask;
    pool.Free(book[idx]);
    book[idx] = pool.Allocate(idx, 10125, 100);
  }
}

int main() {
  SlabPool<Order> pool(kBookSlots + 256);
  std::vector<Order*> book(kBookSlots);
  for (uint32_t i = 0; i < kBookSlots; ++i) book[i] = pool.Allocate(i, 10125, 100);

  constexpr int kWarmup = 5'000, kBursts = 50'000;
  std::vector<double> us;
  us.reserve(kBursts);
  uint32_t base = 0;
  int64_t checksum = 0;
  for (int b = 0; b < kWarmup + kBursts; ++b) {
    auto t0 = std::chrono::steady_clock::now();
    ApplyMutationBurst(pool, book, base);
    auto t1 = std::chrono::steady_clock::now();
    checksum += book[base & kBookMask]->price;
    base = (base * 2654435761u + 256) & kBookMask;  // jump around the book
    if (b >= kWarmup) us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
  }
  std::sort(us.begin(), us.end());
  auto pct = [&](double p) { return us[static_cast<size_t>(p * (us.size() - 1))]; };
  std::printf("C++ slab pool: p50 %.2f us | p99 %.2f us | p99.99 %.2f us | max %.2f us (checksum %lld)\n",
              pct(0.50), pct(0.99), pct(0.9999), us.back(), static_cast<long long>(checksum));
}
