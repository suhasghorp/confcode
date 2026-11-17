// Deopt.java - run with: java -Xlog:deoptimization=debug Deopt.java
public class Deopt {
    abstract static class MarketEvent { abstract long handle(); }

    static final class LimitOrderEvent extends MarketEvent {
        final long px, qty, fee;
        LimitOrderEvent(long px, long qty, long fee) { this.px = px; this.qty = qty; this.fee = fee; }
        long handle() { return px * qty - fee; }
    }

    static final class MarketHaltEvent extends MarketEvent {
        long handle() { return -1; }
    }

    static long sink;

    static long onPacket(MarketEvent ev) { return ev.handle(); }  // C2 bets on LimitOrderEvent

    static void warm(MarketEvent[] limits, int n) {
        for (int i = 0; i < n; ++i) sink += onPacket(limits[i & 1023]);
    }

    static long timeOne(MarketEvent ev) {
        long t0 = System.nanoTime();
        sink += onPacket(ev);
        return System.nanoTime() - t0;
    }

    public static void main(String[] args) throws Exception {
        MarketEvent halt = new MarketHaltEvent();     // load the class before warm-up
        MarketEvent[] limits = new MarketEvent[1024];
        for (int i = 0; i < limits.length; ++i) limits[i] = new LimitOrderEvent(10125, 100 + i % 7, i & 3);

        warm(limits, 100_000);
        Thread.sleep(200);                            // let C2 finish compiling onPacket
        warm(limits, 100_000);

        timeOne(limits[0]);                           // first call of timeOne is cold: discard it
        long limitNs = timeOne(limits[1]);
        long haltNs = timeOne(halt);                  // fails the class guard: deoptimizes
        System.out.printf("LimitOrderEvent: %d ns, first MarketHaltEvent: %d ns%n", limitNs, haltNs);
    }
}
