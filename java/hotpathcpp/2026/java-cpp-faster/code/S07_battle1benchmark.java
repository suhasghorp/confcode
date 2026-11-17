record OrderResult(long notional, long feeTicks) {}

final class LimitOrderEvent extends OrderEvent {
  @Override
  OrderResult processHeapAlloc() {
    return new OrderResult(price * qty, fee);
  }
}

long runBattle1Step(OrderEvent ev) {
  OrderResult res = ev.processHeapAlloc();
  return res.notional() - res.feeTicks();
}
