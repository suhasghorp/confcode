void applyMutationBurst(
    OrderRecord[] book,
    int baseSlot) {
  for (int j = 0; j < 256; ++j) {
    int idx = (baseSlot + j) & BOOK_MASK;
    book[idx] = new OrderRecord(
        idx, 10125L, 100, 1, 0);
  }
}
