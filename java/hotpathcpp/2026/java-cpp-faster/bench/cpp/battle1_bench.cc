// Battle 1 benchmarks. Each benchmark iteration processes one event, so
// Google Benchmark's "Time" column is nanoseconds per event.
#include <benchmark/benchmark.h>

#include <vector>

#include "battle1.h"

// Naive OOP: heap-allocate an event and a result on every iteration.
static void BM_NaiveOOP(benchmark::State& state) {
  size_t i = 0;
  int64_t net_exposure = 0;
  for (auto _ : state) {
    auto ev = CreateOrderEvent(i++);
    auto res = ev->ProcessHeapAlloc();
    net_exposure += res->gross_notional - res->fee_ticks;
  }
  benchmark::DoNotOptimize(net_exposure);
}
BENCHMARK(BM_NaiveOOP);

// Pre-allocated ring of two concrete types; one virtual call per event.
static void BM_PreallocatedRing(benchmark::State& state) {
  constexpr size_t kMask = 1023;
  std::vector<std::unique_ptr<OrderEvent>> ring;
  for (size_t k = 0; k <= kMask; ++k) {
    ring.push_back(CreateOrderEventOfKind(k, k % 8 == 7 ? 1 : 0));
  }
  size_t i = 0;
  int64_t net_exposure = 0;
  for (auto _ : state) {
    OrderResult res = ring[i++ & kMask]->ProcessNoAlloc();
    net_exposure += res.gross_notional - res.fee_ticks;
  }
  benchmark::DoNotOptimize(net_exposure);
}
BENCHMARK(BM_PreallocatedRing);

// Flat 24-byte value structs, five kinds mixed 20% each.
static void BM_FlatValueStruct(benchmark::State& state) {
  constexpr size_t kMask = 1023;
  std::vector<FlatOrderEvent> ring;
  for (size_t k = 0; k <= kMask; ++k) {
    ring.push_back({10125, static_cast<int64_t>(100 + k % 7),
                    static_cast<int32_t>(k & 7),
                    static_cast<EventKind>(k % 5)});
  }
  size_t i = 0;
  int64_t net_exposure = 0;
  for (auto _ : state) {
    OrderResult res = ProcessFlat(ring[i++ & kMask]);
    net_exposure += res.gross_notional - res.fee_ticks;
    benchmark::DoNotOptimize(net_exposure);
  }
}
BENCHMARK(BM_FlatValueStruct);

BENCHMARK_MAIN();
