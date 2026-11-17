#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <string>

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    using namespace std::chrono_literals;

    // Non-blocking: returns a handle (future) to a result that does not exist yet.
    std::future<std::string> fut =
        std::async(std::launch::async, read_file, std::string("input.txt"));

    // Poll: do other, independent work until the result is ready.
    while (fut.wait_for(10ms) != std::future_status::ready) {
        std::cout << "doing other work while the file loads...\n";
    }

    // Block (here it returns at once, since we polled) and take the value.
    std::cout << fut.get();
}

