import java.lang.management.ManagementFactory;

public class RecordSize {
    record OrderRecord(long orderId, long price, int quantity, int symbolId, int flags) {}

    static Object[] keep = new Object[1_000_000];

    public static void main(String[] args) {
        var mx = (com.sun.management.ThreadMXBean) ManagementFactory.getThreadMXBean();
        long tid = Thread.currentThread().threadId();
        long before = mx.getThreadAllocatedBytes(tid);
        for (int i = 0; i < keep.length; ++i) keep[i] = new OrderRecord(i, 1, 2, 3, 4);
        long after = mx.getThreadAllocatedBytes(tid);
        // Default JDK 21: 40.00. With -XX:-UseCompressedClassPointers: 48.00.
        System.out.printf("%.2f bytes per OrderRecord%n", (after - before) / (double) keep.length);
    }
}
