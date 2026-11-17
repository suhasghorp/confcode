#include "battle1.h"

namespace {

class LimitOrderEvent final : public OrderEvent {
 public:
  LimitOrderEvent(int64_t id, int64_t px, int64_t qty)
      : id_(id), price_ticks_(px), qty_(qty) {}
  std::unique_ptr<OrderResult> ProcessHeapAlloc() const override {
    return std::make_unique<OrderResult>(
        OrderResult{price_ticks_ * qty_, id_ & 7});
  }
  OrderResult ProcessNoAlloc() const override {
    return {price_ticks_ * qty_, id_ & 7};
  }
 private:
  int64_t id_, price_ticks_, qty_;
};

class CancelOrderEvent final : public OrderEvent {
 public:
  explicit CancelOrderEvent(int64_t id) : id_(id) {}
  std::unique_ptr<OrderResult> ProcessHeapAlloc() const override {
    return std::make_unique<OrderResult>(OrderResult{0, id_ & 3});
  }
  OrderResult ProcessNoAlloc() const override { return {0, id_ & 3}; }
 private:
  int64_t id_;
};

}  // namespace

std::unique_ptr<OrderEvent> CreateOrderEvent(size_t index) {
  return std::make_unique<LimitOrderEvent>(static_cast<int64_t>(index),
                                           10125, 100);
}

std::unique_ptr<OrderEvent> CreateOrderEventOfKind(size_t index, int kind) {
  if (kind == 0) return CreateOrderEvent(index);
  return std::make_unique<CancelOrderEvent>(static_cast<int64_t>(index));
}
