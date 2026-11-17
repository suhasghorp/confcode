package bench;

import java.util.ArrayList;
import java.util.SplittableRandom;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.*;

/** Battle 2: scan 4,000,000 orders and sum price * quantity. */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.MILLISECONDS)
@Warmup(iterations = 5, time = 2)
@Measurement(iterations = 5, time = 2)
@Fork(value = 2, jvmArgs = {"-Xms4g", "-Xmx4g"})
@State(Scope.Benchmark)
public class Battle2Bench {

  record OrderRecord(long orderId, long price, int quantity, int symbolId, int flags) {}

  static final int ORDERS = 4_000_000;

  /** fresh: objects allocated in one loop. churned: then 3 replacements per slot plus GCs. */
  @Param({"fresh", "churned"})
  String heap;

  ArrayList<OrderRecord> orders;

  static OrderRecord order(long i) {
    return new OrderRecord(i, 10000 + i % 97, (int) (1 + i % 13), (int) (i % 512), 0);
  }

  @Setup(Level.Trial)
  public void setup() {
    orders = new ArrayList<>(ORDERS);
    for (int i = 0; i < ORDERS; ++i) orders.add(order(i));
    if (heap.equals("churned")) {
      SplittableRandom rnd = new SplittableRandom(42);
      for (int step = 0; step < 3 * ORDERS; ++step) {
        orders.set(rnd.nextInt(ORDERS), order(step));
        if (step % ORDERS == 0) System.gc();
      }
      System.gc();
    }
  }

  static long computeTotalExposure(ArrayList<OrderRecord> orders) {
    long totalNotional = 0;
    for (int i = 0; i < orders.size(); ++i) {
      OrderRecord ord = orders.get(i);
      totalNotional += ord.price() * ord.quantity();
    }
    return totalNotional;
  }

  @Benchmark
  public long arrayListScan() {
    return computeTotalExposure(orders);
  }
}
