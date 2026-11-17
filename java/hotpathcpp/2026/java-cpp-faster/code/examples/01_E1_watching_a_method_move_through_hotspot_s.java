// Tiers.java - run with: java -XX:+PrintCompilation Tiers.java
public class Tiers {
    interface Event { long process(); }

    record Limit(long px, long qty) implements Event {
        public long process() { return px * qty; }
    }

    static long step(Event e) {
        return e.process();          // a virtual (interface) call
    }

    public static void main(String[] args) {
        Event e = new Limit(10125, 100);
        long sum = 0;
        for (int i = 0; i < 5_000_000; i++) {
            sum += step(e);          // hot: compiled at tier 3, then tier 4
        }
        System.out.println(sum);
    }
}
