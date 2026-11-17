#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stop_token>
#include <thread>

int main()
{
    std::mutex m;
    std::condition_variable_any cv;
    using namespace std::chrono_literals;

    std::jthread worker([&](std::stop_token st) {          // first parameter: jthread passes its token
        std::unique_lock lock(m);
        int ticks = 0;
        while (!st.stop_requested()) {
            ++ticks;
            cv.wait_for(lock, st, 100ms, [] { return false; });   // returns early on stop request
        }
        std::cout << "stopped after " << ticks << " ticks\n";
    });

    std::this_thread::sleep_for(350ms);
    worker.request_stop();                                  // wakes the wait immediately
}                                                           // jthread joins here
