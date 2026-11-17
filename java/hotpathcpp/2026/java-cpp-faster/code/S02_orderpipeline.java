record OrderResult(long grossNotional, long feeTicks) {}

abstract class OrderEvent {
  abstract OrderResult processHeapAlloc();
}

long runOrderPipeline(long n) {
  long netExposure = 0;
  for (long i = 0; i < n; ++i) {
    OrderEvent ev = createOrderEvent(i);
    OrderResult res = ev.processHeapAlloc();
    netExposure += res.grossNotional() - res.feeTicks();
  }
  return netExposure;
}
