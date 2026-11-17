for (int i = 0; i < 4_000_000; ++i) {
  orders.add(new OrderRecord(i, p, q, s, 0));
}
long exposure = computeTotalExposure(orders);
