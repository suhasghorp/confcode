#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };

class Event {
 public:
  virtual ~Event() = default;
  virtual OrderResult ProcessNoAlloc() const = 0;
};

class LimitOrderEvent final : public Event {
 public:
  LimitOrderEvent(int64_t px, int64_t qty, int64_t fee) : px_(px), qty_(qty), fee_(fee) {}
  OrderResult ProcessNoAlloc() const override { return {px_ * qty_, fee_}; }
 private:
  int64_t px_, qty_, fee_;
};

// The element type is the final class, so ProcessNoAlloc has exactly one
// possible target: GCC calls (and inlines) it directly, no vtable load.
int64_t RunLimitRing(const std::vector<std::unique_ptr<LimitOrderEvent>>& ring) {
  int64_t net_exposure = 0;
  for (const auto& ev : ring) {
    OrderResult res = ev->ProcessNoAlloc();
    net_exposure += res.gross_notional - res.fee_ticks;
  }
  return net_exposure;
}

int main() {
  std::vector<std::unique_ptr<LimitOrderEvent>> ring;
  for (int i = 0; i < 1024; ++i) ring.push_back(std::make_unique<LimitOrderEvent>(10125, 100 + i % 7, i & 7));
  std::printf("%lld\n", static_cast<long long>(RunLimitRing(ring)));
}
