package bench;

import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.*;

/** Battle 1: one virtual call that returns a freshly allocated result object. */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@Warmup(iterations = 5, time = 1)
@Measurement(iterations = 5, time = 1)
@Fork(2)
@State(Scope.Thread)
public class Battle1Bench {

  record OrderResult(long grossNotional, long feeTicks) {}

  abstract static class OrderEvent {
    abstract OrderResult processHeapAlloc();
  }

  static final class LimitOrderEvent extends OrderEvent {
    final long id, price, qty;
    LimitOrderEvent(long id, long price, long qty) { this.id = id; this.price = price; this.qty = qty; }
    @Override OrderResult processHeapAlloc() { return new OrderResult(price * qty, id & 7); }
  }
  static final class CancelOrderEvent extends OrderEvent {
    final long id;
    CancelOrderEvent(long id) { this.id = id; }
    @Override OrderResult processHeapAlloc() { return new OrderResult(0, id & 3); }
  }
  static final class ModifyOrderEvent extends OrderEvent {
    final long id, delta;
    ModifyOrderEvent(long id, long delta) { this.id = id; this.delta = delta; }
    @Override OrderResult processHeapAlloc() { return new OrderResult(delta * 100, id & 1); }
  }
  static final class FillOrderEvent extends OrderEvent {
    final long px, filled;
    FillOrderEvent(long px, long filled) { this.px = px; this.filled = filled; }
    @Override OrderResult processHeapAlloc() { return new OrderResult(px * filled, 2); }
  }
  static final class PegOrderEvent extends OrderEvent {
    final long ref, offset;
    PegOrderEvent(long ref, long offset) { this.ref = ref; this.offset = offset; }
    @Override OrderResult processHeapAlloc() { return new OrderResult(ref + offset, 1); }
  }

  static OrderEvent make(int kind, long i) {
    return switch (kind) {
      case 0 -> new LimitOrderEvent(i, 10125L, 100L + i % 7);
      case 1 -> new CancelOrderEvent(i);
      case 2 -> new ModifyOrderEvent(i, i % 11);
      case 3 -> new FillOrderEvent(10125L, i % 50);
      default -> new PegOrderEvent(10125L, i % 3);
    };
  }

  /** Number of concrete event types seen at the call site: 1, 2 or 5. */
  @Param({"1", "2", "5"})
  int types;

  static final int MASK = 1023;
  OrderEvent[] ring;
  int i;

  @Setup
  public void setup() {
    ring = new OrderEvent[MASK + 1];
    for (int k = 0; k <= MASK; ++k) ring[k] = make(k % types, k);
  }

  @Benchmark
  public long preallocatedRing() {
    OrderEvent ev = ring[i++ & MASK];
    OrderResult res = ev.processHeapAlloc();
    return res.grossNotional() - res.feeTicks();
  }
}
