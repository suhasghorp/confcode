#include <algorithm>
#include <deque>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

// The simplest possible event loop: a queue of tasks run by one thread.
class EventLoop {
public:
    void post(std::function<void()> task) { queue_.push_back(std::move(task)); }

    void run() {
        while (!queue_.empty()) {               // "wait_for_event()" ...
            auto task = std::move(queue_.front());
            queue_.pop_front();
            task();                             // ... "dispatch(ev)"
        }
    }

private:
    std::deque<std::function<void()>> queue_;
};

// A long computation (sum of next..last) split into non-blocking chunks.
// Each chunk does a bounded amount of work, then re-posts the rest.
void sum_chunked(EventLoop& loop, std::string name,
                 long long next, long long last, long long acc) {
    const long long stop = std::min(next + 1000, last + 1);
    for (; next < stop; ++next) acc += next;

    if (next > last) {
        std::cout << name << " done: " << acc << '\n';
        return;
    }
    std::cout << name << " yielded at " << next << '\n';
    // The loop's "local variables" must travel inside the next task.
    loop.post([&loop, name = std::move(name), next, last, acc]() mutable {
        sum_chunked(loop, std::move(name), next, last, acc);
    });
}

int main() {
    EventLoop loop;
    loop.post([&] { sum_chunked(loop, "A", 1, 3500, 0); });
    loop.post([&] { sum_chunked(loop, "B", 1, 2500, 0); });
    loop.run();   // Output interleaves A and B: fairness without preemption.
}

