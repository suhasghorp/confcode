package bench;

/**
 * Warm a call site on LimitOrderEvent only, then send one MarketHaltEvent.
 * The first rare event fails C2's speculative class guard and deoptimizes.
 */
public final class DeoptTrap {

  abstract static class MarketEvent {
    abstract long handle();
  }

  static final class LimitOrderEvent extends MarketEvent {
    final long px, qty, fee;
    LimitOrderEvent(long px, long qty, long fee) { this.px = px; this.qty = qty; this.fee = fee; }
    @Override long handle() { return px * qty - fee; }
  }

  static final class MarketHaltEvent extends MarketEvent {
    final long venue;
    MarketHaltEvent(long venue) { this.venue = venue; }
    @Override long handle() { return -venue; }
  }

  static long sink;

  static long onPacket(MarketEvent ev) {
    return ev.handle();
  }

  // The warm-up loop lives in its own method, so main() itself stays in
  // the interpreter and calls the C2-compiled onPacket() directly.
  static void warm(MarketEvent[] limits, int n) {
    for (int i = 0; i < n; ++i) sink += onPacket(limits[i & 1023]);
  }

  static long timeOne(MarketEvent ev) {
    long t0 = System.nanoTime();
    sink += onPacket(ev);
    return System.nanoTime() - t0;
  }

  public static void main(String[] args) throws Exception {
    // Load MarketHaltEvent up front, so the guard is a type-profile bet,
    // not a class-hierarchy assumption that class loading would break.
    MarketEvent halt = new MarketHaltEvent(7);
    MarketEvent[] limits = new MarketEvent[1024];
    for (int i = 0; i < limits.length; ++i) limits[i] = new LimitOrderEvent(10125, 100 + i % 7, i & 3);

    warm(limits, 100_000);
    Thread.sleep(200);  // let the background C2 compiles finish
    warm(limits, 100_000);

    long base1 = timeOne(limits[1]);
    long base2 = timeOne(limits[2]);
    long first = timeOne(halt);
    long second = timeOne(halt);
    long after = timeOne(limits[3]);

    System.out.printf("LimitOrderEvent: %d, %d ns | first MarketHaltEvent: %d ns | second: %d ns | next LimitOrderEvent: %d ns (sink %d)%n",
        base1, base2, first, second, after, sink);
  }
}
