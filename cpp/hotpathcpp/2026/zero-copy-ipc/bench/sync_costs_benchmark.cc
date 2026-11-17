// What does one Advance() pay for? Costs of the individual operations,
// measured in a single thread (as in the repo's benchmark).
#include <benchmark/benchmark.h>

#include <atomic>
#include <cstdint>
#include <ctime>

#include "absl/time/clock.h"
#include "absl/time/time.h"

static void BM_AbslNowToUnixNanos(benchmark::State& state) {
  for (auto _ : state) benchmark::DoNotOptimize(absl::ToUnixNanos(absl::Now()));
}
BENCHMARK(BM_AbslNowToUnixNanos);

static void BM_ClockGettimeMonotonic(benchmark::State& state) {  // vDSO, no syscall
  timespec ts;
  for (auto _ : state) {
    clock_gettime(CLOCK_MONOTONIC, &ts);
    benchmark::DoNotOptimize(ts);
  }
}
BENCHMARK(BM_ClockGettimeMonotonic);

static void BM_ClockGettimeProcessCpu(benchmark::State& state) {  // real syscall
  timespec ts;
  for (auto _ : state) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    benchmark::DoNotOptimize(ts);
  }
}
BENCHMARK(BM_ClockGettimeProcessCpu);

static void BM_UncontendedCas(benchmark::State& state) {  // lock cmpxchg, line in own L1
  std::atomic<uint32_t> gen{14};
  for (auto _ : state) {
    uint32_t expected = 14;
    benchmark::DoNotOptimize(gen.compare_exchange_strong(expected, 14, std::memory_order_acquire));
  }
}
BENCHMARK(BM_UncontendedCas);

static void BM_ReleaseStore(benchmark::State& state) {  // plain mov on x86-64
  std::atomic<uint64_t> cursor{0};
  uint64_t v = 0;
  for (auto _ : state) cursor.store(++v, std::memory_order_release);
  benchmark::DoNotOptimize(cursor);
}
BENCHMARK(BM_ReleaseStore);

static void BM_PauseResumeTiming(benchmark::State& state) {  // what each batch iteration adds
  for (auto _ : state) {
    state.PauseTiming();
    state.ResumeTiming();
  }
}
BENCHMARK(BM_PauseResumeTiming);

BENCHMARK_MAIN();
