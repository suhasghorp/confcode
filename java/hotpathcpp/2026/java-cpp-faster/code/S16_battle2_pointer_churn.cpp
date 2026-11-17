void BM_Battle2_PointerArray_ChurnedHeap(
    benchmark::State& state) {
  std::vector<const Order*> ptrs = MakeShuffledHeap();
  for (auto _ : state) {
    int64_t total_notional = 0;
    for (size_t i = 0; i < ptrs.size(); ++i) {
      total_notional += ptrs[i]->price * ptrs[i]->quantity;
    }
    benchmark::DoNotOptimize(total_notional);
  }
}
