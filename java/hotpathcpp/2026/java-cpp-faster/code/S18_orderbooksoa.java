final class OrderBookSoA {
  final long[] prices;
  final long[] quantities;
  final long[] masks;

  long scanOrderBookSoA(int n) {
    long total = 0L;
    for (int i = 0; i < n; ++i) {
      total += (prices[i] * quantities[i]) & masks[i];
    }
    return total;
  }
}
