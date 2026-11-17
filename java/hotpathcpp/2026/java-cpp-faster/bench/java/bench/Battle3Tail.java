package bench;

import java.util.Arrays;

/**
 * Battle 3 (Java side): 2^21 resting orders, 50,000 bursts of 256 replacements.
 * Prints p50/p99/p99.99/max of the per-burst latency in microseconds.
 * Run it once per collector, e.g. -XX:+UseG1GC or -XX:+UseZGC -XX:+ZGenerational.
 */
public final class Battle3Tail {

  record OrderRecord(long orderId, long price, int quantity, int symbolId, int flags) {}

  static final int BOOK_SLOTS = 1 << 21;
  static final int BOOK_MASK = BOOK_SLOTS - 1;

  static void applyMutationBurst(OrderRecord[] book, int baseSlot) {
    for (int j = 0; j < 256; ++j) {
      int idx = (baseSlot + j) & BOOK_MASK;
      book[idx] = new OrderRecord(idx, 10125L, 100, 1, 0);
    }
  }

  public static void main(String[] args) {
    OrderRecord[] book = new OrderRecord[BOOK_SLOTS];
    for (int i = 0; i < BOOK_SLOTS; ++i) book[i] = new OrderRecord(i, 10125L, 100, 1, 0);

    final int warmup = 5_000, bursts = 50_000;
    double[] us = new double[bursts];
    int base = 0;
    long checksum = 0;
    for (int b = 0; b < warmup + bursts; ++b) {
      long t0 = System.nanoTime();
      applyMutationBurst(book, base);
      long t1 = System.nanoTime();
      checksum += book[base & BOOK_MASK].price();
      base = (int) ((base * 2654435761L + 256) & BOOK_MASK);
      if (b >= warmup) us[b - warmup] = (t1 - t0) / 1000.0;
    }
    Arrays.sort(us);
    System.out.printf("Java %s: p50 %.2f us | p99 %.2f us | p99.99 %.2f us | max %.2f us (checksum %d)%n",
        args.length > 0 ? args[0] : "", us[(int) (0.50 * (bursts - 1))], us[(int) (0.99 * (bursts - 1))],
        us[(int) (0.9999 * (bursts - 1))], us[bursts - 1], checksum);
  }
}
