#include <chrono>
#include <iostream>
#include <latch>
#include <thread>

int main()
{
    using namespace std::chrono_literals;
    std::latch ready{1};

    std::jthread worker([&] {
        std::this_thread::sleep_for(100ms);   // worker_thread's own work
        std::this_thread::sleep_for(900ms);   // child_function
        ready.count_down();                   // instead of ready_flag.store(true)
    });

    std::jthread waiter([&] {
        std::cout << "[Thread 1] Waiting for ready signal..." << std::endl;
        ready.wait();                         // blocks without burning CPU
        std::cout << "[Thread 1] Got signal! Exiting." << std::endl;
    });
}
