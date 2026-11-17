class LimitOrderEvent final : public OrderEvent {
 private:
  int64_t id_, price_ticks_, qty_;
};

int64_t RunOrderService(size_t n) {
  int64_t net_exposure = 0;
  for (size_t i = 0; i < n; ++i) {
    auto ev = CreateOrderEvent(i);
    auto res = ev->ProcessHeapAlloc();
    net_exposure += res->gross_notional - res->fee_ticks;
  }
  return net_exposure;
}
