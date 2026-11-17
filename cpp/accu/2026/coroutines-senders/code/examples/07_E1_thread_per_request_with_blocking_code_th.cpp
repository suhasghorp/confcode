#include <chrono>
#include <iostream>
#include <string>
#include <syncstream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// Simulates a blocking I/O call, e.g. a socket read.
// While this thread sleeps, the OS context-switches to other threads.
std::string blocking_fetch(int id) {
    std::this_thread::sleep_for(200ms);
    return "response " + std::to_string(id);
}

void handle_request(int id) {
    // Plain sequential code: each line may rely on the previous one.
    std::string data = blocking_fetch(id);
    std::osyncstream(std::cout) << "request " << id << " -> " << data << '\n';
}

int main() {
    auto start = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> workers;
        for (int id = 0; id < 3; ++id)
            workers.emplace_back(handle_request, id);  // fork
    }   // jthread destructors join here
    auto elapsed = std::chrono::steady_clock::now() - start;
    // About 200ms in total, not 600ms: the OS overlapped the waits.
    std::cout << "elapsed: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
              << "ms\n";
}
