void ApplyMutationBurst(
    SlabPool<Order>& pool,
    std::span<Order*> book,
    uint32_t base_slot) {
  for (uint32_t j = 0; j < 256; ++j) {
    uint32_t idx = (base_slot + j) & kBookMask;
    pool.Free(book[idx]);
    book[idx] = pool.Allocate(idx, 10125, 100);
  }
}
