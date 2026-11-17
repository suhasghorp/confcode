import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.ValueLayout;

// JDK 21: javac --release 21 --enable-preview ...; java --enable-preview ...
// JDK 22 and later: no flags needed.
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

  long priceAt(int slot) {
    return slab.get(ValueLayout.JAVA_LONG_UNALIGNED, (long) slot * ORDER_STRIDE + 8L);
  }

  public static void main(String[] args) {
    OffHeapOrderBook book = new OffHeapOrderBook();
    for (int s = 0; s < 2_097_152; ++s) book.replaceOrderAtSlot(s, s, 10125, 100);
    System.out.println("price at slot 42 = " + book.priceAt(42));
  }
}
