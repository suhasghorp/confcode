// Battle 1: polymorphic order events, declared here and defined in a
// separate translation unit (battle1_factory.cc), as in the talk.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };

class OrderEvent {
 public:
  virtual ~OrderEvent() = default;
  virtual std::unique_ptr<OrderResult> ProcessHeapAlloc() const = 0;
  virtual OrderResult ProcessNoAlloc() const = 0;
};

// Defined in battle1_factory.cc: the caller cannot see the concrete type.
std::unique_ptr<OrderEvent> CreateOrderEvent(size_t index);
// kind 0 = LimitOrderEvent, 1 = CancelOrderEvent.
std::unique_ptr<OrderEvent> CreateOrderEventOfKind(size_t index, int kind);

enum class EventKind : uint8_t { kLimit, kCancel, kModify, kFill, kPeg };

struct FlatOrderEvent {
  int64_t price_ticks;
  int64_t qty;
  int32_t fee_bps;
  EventKind kind;
};
static_assert(sizeof(FlatOrderEvent) == 24);

inline OrderResult ProcessFlat(const FlatOrderEvent& e) {
  return {e.price_ticks * e.qty, e.fee_bps};
}
