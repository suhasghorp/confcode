#include <chrono>
#include <iostream>
#include <stop_token>
#include <thread>

int main() {
    using namespace std::chrono_literals;

    std::jthread worker([](std::stop_token st) {
        int ticks = 0;
        while (!st.stop_requested()) {   // cooperative cancellation point
            ++ticks;
            std::this_thread::sleep_for(10ms);
        }
        std::cout << "worker stopped after " << ticks << " ticks\n";
    });

    std::this_thread::sleep_for(100ms);
    // No explicit join: ~jthread() calls request_stop(), then join().
}

