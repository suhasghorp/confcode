for (int step = 0; step < kChurnOps; ++step) {
  orders.set(randomSlot(), new OrderRecord(step, p, q, s, 0));
}
long exposure = computeTotalExposure(orders);
