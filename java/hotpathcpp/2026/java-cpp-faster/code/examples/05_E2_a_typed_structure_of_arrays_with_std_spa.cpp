#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

// Owns three parallel columns and keeps them the same length.
class OrderColumns {
 public:
  void Add(int64_t price, int64_t qty, bool active) {
    prices_.push_back(price);
    quantities_.push_back(qty);
    masks_.push_back(active ? -1 : 0);
  }
  std::size_t size() const { return prices_.size(); }
  std::span<const int64_t> prices() const { return prices_; }
  std::span<const int64_t> quantities() const { return quantities_; }
  std::span<const int64_t> masks() const { return masks_; }

 private:
  std::vector<int64_t> prices_, quantities_, masks_;
};

int64_t TotalActiveNotional(const OrderColumns& book) {
  auto px = book.prices();
  auto qty = book.quantities();
  auto mask = book.masks();
  int64_t total = 0;
  for (std::size_t i = 0; i < px.size(); ++i) {
    total += (px[i] * qty[i]) & mask[i];   // vectorizes at -O3
  }
  return total;
}

int main() {
  OrderColumns book;
  for (int i = 0; i < 1000; ++i) book.Add(10000 + i % 97, 1 + i % 13, i % 10 != 0);
  std::printf("%zu orders, active notional %lld\n", book.size(),
              static_cast<long long>(TotalActiveNotional(book)));
}
