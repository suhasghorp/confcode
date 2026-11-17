record OrderResult(long grossNotional, long feeTicks) {}

long processCustomerOrder(OrderEvent event) {
  OrderResult res = event.processHeapAlloc();
  return res.grossNotional() - res.feeTicks();
}
