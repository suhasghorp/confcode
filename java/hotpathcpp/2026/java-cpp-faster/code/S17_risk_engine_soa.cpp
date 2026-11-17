struct OrderBookSoA {
  const int64_t* __restrict prices;
  const int64_t* __restrict quantities;
  const int64_t* __restrict masks;
};

int64_t ScanOrderBookSoA(const OrderBookSoA& b, size_t n) {
  int64_t total = 0;
  for (size_t i = 0; i < n; ++i) {
    total += (b.prices[i] * b.quantities[i]) & b.masks[i];
  }
  return total;
}
