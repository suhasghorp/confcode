#include <stop_token>
#include <thread>

void with_thread() {
    std::thread t([] { /* work */ });
    // If we return or throw here before join(), ~thread() sees a
    // joinable thread and calls std::terminate.
    t.join();  // must remember this on every path
}

void with_jthread() {
    std::jthread t([](std::stop_token st) {
        while (!st.stop_requested()) {
            /* do a small piece of work */
        }
    });
    // ... any code, including an early return or an exception ...
}   // ~jthread(): request_stop(), then join(). The task cannot outlive t.

int main() {
    with_thread();
    with_jthread();
}
