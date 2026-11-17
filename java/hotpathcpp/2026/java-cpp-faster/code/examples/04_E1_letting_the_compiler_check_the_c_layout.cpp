#include <cstddef>
#include <cstdint>

struct Order {
  int64_t order_id;
  int64_t price;
  int32_t quantity;
  int32_t symbol_id;
  int32_t flags;
};

// 28 bytes of fields, rounded up to the 8-byte alignment of int64_t.
static_assert(sizeof(Order) == 32);
static_assert(alignof(Order) == 8);
static_assert(offsetof(Order, flags) == 24);   // 4 bytes of tail padding follow
static_assert(64 / sizeof(Order) == 2);        // two complete orders per cache line

int main() {}
