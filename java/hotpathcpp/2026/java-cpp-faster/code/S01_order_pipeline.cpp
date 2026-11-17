struct OrderResult { int64_t gross_notional; int64_t fee_ticks; };
class OrderEvent {
 public:
  virtual ~OrderEvent() = default;
  virtual std::unique_ptr<OrderResult> ProcessHeapAlloc() const = 0;
};
int64_t RunOrderPipeline(size_t n) {
  int64_t net_exposure = 0;
  for (size_t i = 0; i < n; ++i) {
    auto ev = CreateOrderEvent(i);
    auto res = ev->ProcessHeapAlloc();
    net_exposure += res->gross_notional - res->fee_ticks;
  }
  return net_exposure;
}
