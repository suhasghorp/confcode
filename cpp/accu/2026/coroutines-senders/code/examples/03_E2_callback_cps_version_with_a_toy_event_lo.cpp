#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

// A minimal single-threaded "event loop": a queue of pending tasks.
std::deque<std::function<void()>> pending;

// Non-blocking: schedules the read and returns immediately.
// 'on_done' is the continuation: the code that truly depends on the result.
void async_read_file(std::string path,
                     std::function<void(std::string)> on_done) {
    pending.push_back([path = std::move(path), on_done = std::move(on_done)] {
        std::ifstream in(path);
        std::ostringstream ss;
        ss << in.rdbuf();
        on_done(ss.str());   // invoke the continuation with the result
    });
}

void run_event_loop() {
    while (!pending.empty()) {
        auto task = std::move(pending.front());
        pending.pop_front();
        task();
    }
}

int main() {
    std::string data;

    async_read_file("input.txt", [&data](std::string contents) {
        data = std::move(contents);
        std::cout << data;          // correct: runs once the data exists
    });

    // BUG: this line runs before the read has happened.
    // 'data' is still empty here, so this prints 0.
    std::cout << "size right after the call: " << data.size() << '\n';

    run_event_loop();               // only now are the read and callback executed
}

