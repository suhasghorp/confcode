package bench;

import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.*;

/** Battle 1, "fresh in-loop": allocate the event and the result every time. */
@BenchmarkMode(Mode.AverageTime)
@OutputTimeUnit(TimeUnit.NANOSECONDS)
@Warmup(iterations = 5, time = 1)
@Measurement(iterations = 5, time = 1)
@Fork(2)
@State(Scope.Thread)
public class Battle1FreshBench {

  long i;

  @Benchmark
  public long freshInLoop() {
    Battle1Bench.OrderEvent ev = new Battle1Bench.LimitOrderEvent(i++, 10125L, 100L);
    Battle1Bench.OrderResult res = ev.processHeapAlloc();
    return res.grossNotional() - res.feeTicks();
  }
}
