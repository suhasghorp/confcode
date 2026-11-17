struct Order {
  int64_t order_id;
  int64_t price;
  int32_t quantity;
  int32_t symbol_id;
  int32_t flags;
};

int64_t ComputeTotalExposure(
    const std::vector<Order>& orders) {
  int64_t total_notional = 0;
  for (size_t i = 0; i < orders.size(); ++i) {
    total_notional += orders[i].price * orders[i].quantity;
  }
  return total_notional;
}
