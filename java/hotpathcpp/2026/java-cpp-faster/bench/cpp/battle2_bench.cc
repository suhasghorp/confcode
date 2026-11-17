// Battle 2: scan 4,000,000 orders and sum price * quantity.
// "Time" is milliseconds per full scan.
#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

constexpr size_t kOrders = 4'000'000;

struct Order {
  int64_t order_id;
  int64_t price;
  int32_t quantity;
  int32_t symbol_id;
  int32_t flags;
};
static_assert(sizeof(Order) == 32);

int64_t ComputeTotalExposure(const std::vector<Order>& orders) {
  int64_t total_notional = 0;
  for (size_t i = 0; i < orders.size(); ++i) {
    total_notional += orders[i].price * orders[i].quantity;
  }
  return total_notional;
}

static void BM_Battle2_Vector(benchmark::State& state) {
  std::vector<Order> orders(kOrders);
  for (size_t i = 0; i < kOrders; ++i) {
    orders[i] = {static_cast<int64_t>(i), 10000 + static_cast<int64_t>(i % 97),
                 static_cast<int32_t>(1 + i % 13), static_cast<int32_t>(i % 512), 0};
  }
  for (auto _ : state) {
    benchmark::DoNotOptimize(ComputeTotalExposure(orders));
  }
}
BENCHMARK(BM_Battle2_Vector)->Unit(benchmark::kMillisecond);

// One heap allocation per order, then the pointer array is shuffled so
// that consecutive pointers land on unrelated cache lines and pages.
static void BM_Battle2_PointerArray_ChurnedHeap(benchmark::State& state) {
  std::vector<std::unique_ptr<Order>> owner;
  owner.reserve(kOrders);
  for (size_t i = 0; i < kOrders; ++i) {
    owner.push_back(std::make_unique<Order>(
        Order{static_cast<int64_t>(i), 10000 + static_cast<int64_t>(i % 97),
              static_cast<int32_t>(1 + i % 13), static_cast<int32_t>(i % 512), 0}));
  }
  std::vector<const Order*> ptrs;
  ptrs.reserve(kOrders);
  for (auto& p : owner) ptrs.push_back(p.get());
  std::shuffle(ptrs.begin(), ptrs.end(), std::mt19937_64{42});
  for (auto _ : state) {
    int64_t total_notional = 0;
    for (size_t i = 0; i < ptrs.size(); ++i) {
      total_notional += ptrs[i]->price * ptrs[i]->quantity;
    }
    benchmark::DoNotOptimize(total_notional);
  }
}
BENCHMARK(BM_Battle2_PointerArray_ChurnedHeap)->Unit(benchmark::kMillisecond);

struct OrderBookSoA {
  const int64_t* __restrict prices;
  const int64_t* __restrict quantities;
  const int64_t* __restrict masks;
};

int64_t ScanOrderBookSoA(const OrderBookSoA& b, size_t n) {
  int64_t total = 0;
  for (size_t i = 0; i < n; ++i) {
    total += (b.prices[i] * b.quantities[i]) & b.masks[i];
  }
  return total;
}

static void BM_Battle2_SoA(benchmark::State& state) {
  std::vector<int64_t> prices(kOrders), quantities(kOrders), masks(kOrders);
  for (size_t i = 0; i < kOrders; ++i) {
    prices[i] = 10000 + static_cast<int64_t>(i % 97);
    quantities[i] = 1 + static_cast<int64_t>(i % 13);
    masks[i] = (i % 10 == 0) ? 0 : -1;  // 10% of orders inactive
  }
  OrderBookSoA book{prices.data(), quantities.data(), masks.data()};
  for (auto _ : state) {
    benchmark::DoNotOptimize(ScanOrderBookSoA(book, kOrders));
  }
}
BENCHMARK(BM_Battle2_SoA)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
