#include <deque>
#include <functional>
#include <iostream>

// The "reactor": a queue of ready-to-run tasks.
std::deque<std::function<void()>> ready;

// A fake non-blocking operation: it only *schedules* the callback and returns.
void async_tick(int n, std::function<void(int)> on_done) {
    ready.push_back([n, cb = std::move(on_done)] { cb(n + 1); });
}

void do_ping(int n);

void do_pong(int n) {
    async_tick(n, [](int m) { do_ping(m); });   // continuation = next task
}

void do_ping(int n) {
    if (n >= 1'000'000) {
        std::cout << "done at " << n << '\n';
        return;
    }
    async_tick(n, [](int m) { do_pong(m); });
    // Code here runs BEFORE the tick has completed!
}

int main() {
    do_ping(0);                       // returns almost immediately
    while (!ready.empty()) {          // the event loop
        auto task = std::move(ready.front());
        ready.pop_front();
        task();                       // stack depth stays small: no real recursion
    }
}

