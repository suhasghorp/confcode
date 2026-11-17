import java.lang.management.ManagementFactory;

public class EscapeDemo {
    record OrderResult(long grossNotional, long feeTicks) {}

    static final class LimitOrderEvent {
        final long id, price, qty;
        LimitOrderEvent(long id, long price, long qty) { this.id = id; this.price = price; this.qty = qty; }
        OrderResult process() { return new OrderResult(price * qty, id & 7); }
    }

    static long run(long n) {
        long net = 0;
        for (long i = 0; i < n; ++i) {
            LimitOrderEvent ev = new LimitOrderEvent(i, 10125, 100);  // never escapes
            OrderResult res = ev.process();                           // never escapes
            net += res.grossNotional() - res.feeTicks();
        }
        return net;
    }

    public static void main(String[] args) {
        var mx = (com.sun.management.ThreadMXBean) ManagementFactory.getThreadMXBean();
        long tid = Thread.currentThread().threadId();
        for (int warm = 0; warm < 20; ++warm) run(1_000_000);      // let C2 compile run()
        long before = mx.getThreadAllocatedBytes(tid);
        long sum = run(10_000_000);
        long after = mx.getThreadAllocatedBytes(tid);
        System.out.printf("%,d bytes allocated for 10,000,000 iterations (%.2f bytes/iteration), sum=%d%n",
                after - before, (after - before) / 1e7, sum);
    }
}
