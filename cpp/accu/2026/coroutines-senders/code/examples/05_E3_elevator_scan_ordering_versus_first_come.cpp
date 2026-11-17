#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <vector>

// Serve every request above the head while moving up, then the rest moving down.
std::vector<int> scan_order(int head, std::vector<int> requests) {
    std::ranges::sort(requests);
    auto split = std::ranges::lower_bound(requests, head);
    std::vector<int> order(split, requests.end());                 // upward sweep
    order.insert(order.end(), std::make_reverse_iterator(split),   // downward sweep
                 requests.rend());
    return order;
}

int total_travel(int head, const std::vector<int>& order) {
    int travel = 0;
    for (int track : order) {
        travel += std::abs(track - head);
        head = track;
    }
    return travel;
}

int main() {
    const int head = 50;
    const std::vector<int> arrivals{95, 10, 60, 20, 90, 55};  // arrival order

    std::cout << "FIFO travel: " << total_travel(head, arrivals) << '\n';  // 325
    std::cout << "SCAN travel: "
              << total_travel(head, scan_order(head, arrivals)) << '\n';   // 130
}

