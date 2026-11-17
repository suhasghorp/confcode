#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>

template <typename T>
class SlabPool {
 public:
  explicit SlabPool(std::size_t capacity) : slots_(new Slot[capacity]) {
    std::memset(static_cast<void*>(slots_), 0, capacity * sizeof(Slot));  // pre-fault every page
    for (std::size_t i = 0; i + 1 < capacity; ++i) slots_[i].next = &slots_[i + 1];
    slots_[capacity - 1].next = nullptr;
    free_ = slots_;
  }
  ~SlabPool() { delete[] slots_; }
  SlabPool(const SlabPool&) = delete;
  SlabPool& operator=(const SlabPool&) = delete;

  template <typename... Args>
  T* Allocate(Args&&... args) {
    if (free_ == nullptr) return nullptr;            // pool exhausted
    Slot* s = std::exchange(free_, free_->next);     // pop the free list
    return ::new (static_cast<void*>(s->storage)) T{std::forward<Args>(args)...};
  }

  void Free(T* p) {
    if (p == nullptr) return;
    p->~T();
    Slot* s = reinterpret_cast<Slot*>(p);            // storage is at offset 0 of the union
    s->next = std::exchange(free_, s);               // push onto the free list
  }

 private:
  union Slot {
    Slot* next;                                      // while free
    alignas(T) unsigned char storage[sizeof(T)];     // while in use
  };
  Slot* slots_;
  Slot* free_ = nullptr;
};

struct Order {
  int64_t order_id;
  int64_t price;
  int32_t quantity;
  int32_t symbol_id;
  int32_t flags;
};

int main() {
  SlabPool<Order> pool(4);
  Order* a = pool.Allocate(int64_t{1}, int64_t{10125}, 100, 7, 0);
  pool.Free(a);
  Order* b = pool.Allocate(int64_t{2}, int64_t{10130}, 50, 7, 0);   // reuses a's slot
  std::printf("same slot reused: %s, price %lld\n", a == b ? "yes" : "no",
              static_cast<long long>(b->price));
}
