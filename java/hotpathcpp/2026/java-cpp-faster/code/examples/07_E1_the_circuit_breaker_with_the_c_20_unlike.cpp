#include <cstdint>
#include <cstdio>

enum class EventKind : uint8_t { kLimit, kCancel, kModify, kFill, kPeg, kMarketHalt };

struct FlatOrderEvent {
  int64_t price_ticks;
  int64_t qty;
  int32_t fee_bps;
  EventKind kind;
};

int64_t TriggerCircuitBreakerHalt(const FlatOrderEvent&) {
  std::puts("circuit breaker: trading halted");
  return 0;
}

int64_t HandleMarketEvent(const FlatOrderEvent& ev) {
  if (ev.kind == EventKind::kMarketHalt) [[unlikely]] {
    return TriggerCircuitBreakerHalt(ev);   // already machine code, never "first-time" slow
  }
  return ev.price_ticks * ev.qty - ev.fee_bps;
}

int main() {
  int64_t total = 0;
  for (int i = 0; i < 100'000; ++i) {
    total += HandleMarketEvent({10125, 100, 3, EventKind::kLimit});
  }
  total += HandleMarketEvent({0, 0, 0, EventKind::kMarketHalt});
  std::printf("total %lld\n", static_cast<long long>(total));
}
