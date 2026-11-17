// False sharing: two threads on different cores each increment their own
// counter. "packed" puts both counters in one 64-byte line; "aligned" gives
// each counter its own line. Run under the video's perf stat command:
//   perf stat -e cycles,instructions,cache-misses,branch-misses ./false_sharing
#include <sched.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {

struct Packed {
  std::atomic<uint64_t> cursors[2];  // 16 bytes, same cache line
};

struct alignas(64) PaddedCursor {
  std::atomic<uint64_t> value;
};
struct Aligned {
  PaddedCursor cursors[2];  // 128 bytes, one line each
};
static_assert(sizeof(Aligned) == 128);

void PinTo(int cpu) {
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  sched_setaffinity(0, sizeof(set), &set);
}

bool g_rmw = false;  // false: load + release store; true: atomic fetch_add (lock xadd)

template <typename Cursor>
void Bump(Cursor& c, int cpu, long n) {
  PinTo(cpu);
  for (long i = 0; i < n; ++i) {
    if (g_rmw) {
      c.fetch_add(1, std::memory_order_acq_rel);  // read-modify-write, like a CAS
    } else {
      // A consumer publishing its cursor: load, then store.
      c.store(c.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    }
  }
}

template <typename Layout, typename Get>
double Run(Layout& l, Get get, long n) {
  auto t0 = std::chrono::steady_clock::now();
  std::thread a([&] { Bump(get(l, 0), 1, n); });
  std::thread b([&] { Bump(get(l, 1), 2, n); });
  a.join();
  b.join();
  auto t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count() / n;
}

}  // namespace

// Usage: false_sharing [packed|aligned|both] [store|rmw]
int main(int argc, char** argv) {
  constexpr long kN = 100'000'000;
  bool only_packed = argc > 1 && std::strcmp(argv[1], "packed") == 0;
  bool only_aligned = argc > 1 && std::strcmp(argv[1], "aligned") == 0;
  g_rmw = argc > 2 && std::strcmp(argv[2], "rmw") == 0;
  std::printf("pattern: %s\n", g_rmw ? "atomic fetch_add" : "load + release store");
  if (!only_aligned) {
    Packed p{};
    std::printf("packed  (same line):    %.2f ns per update\n",
                Run(p, [](Packed& x, int i) -> auto& { return x.cursors[i]; }, kN));
  }
  if (!only_packed) {
    Aligned a{};
    std::printf("aligned (own line each): %.2f ns per update\n",
                Run(a, [](Aligned& x, int i) -> auto& { return x.cursors[i].value; }, kN));
  }
}
