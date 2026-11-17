#include <atomic>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <stop_token>
#include <thread>
#include <vector>

int main() {
    std::vector<int> data(10'000'000);
    std::iota(data.begin(), data.end(), 0);
    const int target = 7'654'321;

    std::stop_source cancel;                      // "nobody needs this any more"
    std::atomic<std::ptrdiff_t> found{-1};

    auto search = [&](std::size_t lo, std::size_t hi) {
        std::stop_token st = cancel.get_token();
        for (std::size_t i = lo; i < hi; ++i) {
            if (st.stop_requested()) return;      // someone else already won
            if (data[i] == target) {
                found = static_cast<std::ptrdiff_t>(i);
                cancel.request_stop();            // the other search is now unneeded
                return;
            }
        }
    };

    {
        std::stop_callback on_cancel(cancel.get_token(),
                                     [] { std::cout << "cancellation requested\n"; });
        std::jthread a(search, std::size_t{0}, data.size() / 2);
        std::jthread b(search, data.size() / 2, data.size());
    }   // join point: both threads have finished here

    std::cout << "found at index " << found.load() << '\n';
}

