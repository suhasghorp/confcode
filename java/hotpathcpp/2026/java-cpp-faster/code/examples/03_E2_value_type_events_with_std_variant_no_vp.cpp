#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <variant>
#include <vector>

struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };

// Each event type is a plain value: no base class, no vptr, no heap.
struct Limit  { int64_t price_ticks, qty, fee; };
struct Cancel { int64_t order_id; };
struct Fill   { int64_t price_ticks, filled; };

using OrderEvent = std::variant<Limit, Cancel, Fill>;

OrderResult Process(const OrderEvent& ev) {
  return std::visit([](const auto& e) -> OrderResult {
    using T = std::decay_t<decltype(e)>;
    if constexpr (std::is_same_v<T, Limit>)  return {e.price_ticks * e.qty, e.fee};
    if constexpr (std::is_same_v<T, Cancel>) return {0, e.order_id & 3};
    if constexpr (std::is_same_v<T, Fill>)   return {e.price_ticks * e.filled, 2};
  }, ev);
}

int main() {
  std::vector<OrderEvent> events;  // contiguous, sizeof(OrderEvent) bytes each
  for (int64_t i = 0; i < 1'000'000; ++i) {
    switch (i % 3) {
      case 0: events.emplace_back(Limit{10125, 100, i & 7}); break;
      case 1: events.emplace_back(Cancel{i}); break;
      default: events.emplace_back(Fill{10125, i % 50}); break;
    }
  }
  int64_t net_exposure = 0;
  for (const auto& ev : events) {
    OrderResult r = Process(ev);
    net_exposure += r.gross_notional - r.fee_ticks;
  }
  std::printf("sizeof(OrderEvent) = %zu, net exposure = %lld\n",
              sizeof(OrderEvent), static_cast<long long>(net_exposure));
}
