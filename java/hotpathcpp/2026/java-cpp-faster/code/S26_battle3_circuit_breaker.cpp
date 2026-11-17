int64_t HandleMarketEvent(const FlatOrderEvent& ev) {
  if (__builtin_expect(ev.kind == EventKind::kMarketHalt, 0)) {
    return TriggerCircuitBreakerHalt(ev);
  }
  return ev.price_ticks * ev.qty - ev.fee_bps;
}
