#include <chrono>
#include <iostream>
#include <stop_token>
#include <thread>

void cxx11_style() {
    std::thread t([] { std::cout << "work in a C++11 thread\n"; });
    t.join();  // Forget this (or skip it via an exception) and
               // ~thread() calls std::terminate.
}

void cxx20_style() {
    // jthread passes a stop_token to the callable if it accepts one.
    std::jthread t([](std::stop_token st) {
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "worker observed the stop request\n";
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}   // ~jthread(): request_stop(), then join(). The scope owns the thread.

int main() {
    cxx11_style();
    cxx20_style();
}

