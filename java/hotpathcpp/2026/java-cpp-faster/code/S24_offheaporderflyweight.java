final class OffHeapOrderBook {
  private static final long ORDER_STRIDE = 32L;
  private final MemorySegment slab =
      Arena.ofShared().allocate(2_097_152L * ORDER_STRIDE, 64L);

  void replaceOrderAtSlot(int slot, long id, long px, int qty) {
    long offset = (long) slot * ORDER_STRIDE;
    slab.set(ValueLayout.JAVA_LONG_UNALIGNED, offset + 0L, id);
    slab.set(ValueLayout.JAVA_LONG_UNALIGNED, offset + 8L, px);
    slab.set(ValueLayout.JAVA_INT_UNALIGNED, offset + 16L, qty);
  }
}
