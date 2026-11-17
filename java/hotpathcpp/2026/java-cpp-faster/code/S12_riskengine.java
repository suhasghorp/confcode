record OrderRecord(
    long orderId,
    long price,
    int quantity,
    int symbolId,
    int flags) {}

long computeTotalExposure(
    ArrayList<OrderRecord> orders) {
  long totalNotional = 0;
  for (int i = 0; i < orders.size(); ++i) {
    OrderRecord ord = orders.get(i);
    totalNotional += ord.price() * ord.quantity();
  }
  return totalNotional;
}
