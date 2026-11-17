#include <cstdint>
#include <cstdio>
#include <memory>

struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };

class OrderEvent {
 public:
  virtual ~OrderEvent() = default;
  virtual std::unique_ptr<OrderResult> ProcessHeapAlloc() const = 0;
};

class LimitOrderEvent final : public OrderEvent {
 public:
  LimitOrderEvent(int64_t id, int64_t px, int64_t qty)
      : id_(id), price_ticks_(px), qty_(qty) {}
  std::unique_ptr<OrderResult> ProcessHeapAlloc() const override {
    return std::make_unique<OrderResult>(OrderResult{price_ticks_ * qty_, id_ & 7});
  }
 private:
  int64_t id_, price_ticks_, qty_;
};

// The factory is visible in this translation unit and returns the
// concrete, final type, so the compiler knows exactly what runs.
inline std::unique_ptr<LimitOrderEvent> CreateOrderEvent(size_t i) {
  return std::make_unique<LimitOrderEvent>(static_cast<int64_t>(i), 10125, 100);
}

int64_t RunOrderService(size_t n) {
  int64_t net_exposure = 0;
  for (size_t i = 0; i < n; ++i) {
    auto ev = CreateOrderEvent(i);
    auto res = ev->ProcessHeapAlloc();   // devirtualized: the class is final
    net_exposure += res->gross_notional - res->fee_ticks;
  }
  return net_exposure;
}

int main() { std::printf("%lld\n", static_cast<long long>(RunOrderService(1000))); }
