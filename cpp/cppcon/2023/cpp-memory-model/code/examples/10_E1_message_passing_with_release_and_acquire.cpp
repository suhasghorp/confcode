#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

int value;                       // plain data, published by the flag
std::atomic<bool> done{false};

int very_long_calc() { return 42; }

int main()
{
    std::thread producer([] {
        value = very_long_calc();
        done.store(true, std::memory_order_release);     // everything above stays above
    });
    std::thread consumer([] {
        while (!done.load(std::memory_order_acquire)) {} // everything below stays below
        assert(value == 42);                             // guaranteed: no data race
        std::cout << "got " << value << '\n';
    });
    producer.join();
    consumer.join();
}
