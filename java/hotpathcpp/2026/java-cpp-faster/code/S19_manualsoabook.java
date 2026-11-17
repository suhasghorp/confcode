final class ManualSoABook {
  final long[] orderIds = new long[kMaxOrders];
  final long[] prices = new long[kMaxOrders];
  final int[] quantities = new int[kMaxOrders];
  final int[] symbolIds = new int[kMaxOrders];

  void updateOrder(int slot, long px, int qty) {
    prices[slot] = px;
    quantities[slot] = qty;
  }
}
