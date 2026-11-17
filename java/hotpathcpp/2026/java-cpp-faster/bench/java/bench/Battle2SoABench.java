package bench;

import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.*;

/** Battle 2, structure of arrays: three flat long[] columns. */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.MILLISECONDS)
@Warmup(iterations = 5, time = 2)
@Measurement(iterations = 5, time = 2)
@Fork(2)
@State(Scope.Benchmark)
public class Battle2SoABench {

  static final int ORDERS = 4_000_000;
  long[] prices, quantities, masks;

  @Setup
  public void setup() {
    prices = new long[ORDERS];
    quantities = new long[ORDERS];
    masks = new long[ORDERS];
    for (int i = 0; i < ORDERS; ++i) {
      prices[i] = 10000 + i % 97;
      quantities[i] = 1 + i % 13;
      masks[i] = (i % 10 == 0) ? 0 : -1;
    }
  }

  @Benchmark
  public long scanOrderBookSoA() {
    long total = 0L;
    for (int i = 0; i < ORDERS; ++i) {
      total += (prices[i] * quantities[i]) & masks[i];
    }
    return total;
  }
}
