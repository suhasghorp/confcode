#include <asio.hpp>
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

asio::io_context g_io; // the new reactor, run on its own thread

// NEW: the leaf is now a coroutine ("red").
asio::awaitable<std::string> fetch_config_async() {
    auto ex = co_await asio::this_coro::executor;
    asio::steady_timer t(ex, std::chrono::milliseconds(50)); // stand-in for real async I/O
    co_await t.async_wait(asio::use_awaitable);
    co_return std::string{"threads=4"};
}

// BRIDGE: same blue signature the old blocking function had.
// It stops the coloring cascade: callers do not change.
std::string fetch_config() {
    auto fut = asio::co_spawn(g_io, fetch_config_async(), asio::use_future);
    return fut.get(); // blocks, exactly as the old code did. Never call this on g_io's thread!
}

// UNCHANGED blue callers.
// Without the bridge, load_settings would have to become asio::awaitable<int>
// and co_await, and so would every caller up to main.
int parse_threads(const std::string& s) {
    return std::stoi(s.substr(s.find('=') + 1));
}
int load_settings() { return parse_threads(fetch_config()); }

int main() {
    auto guard = asio::make_work_guard(g_io);   // keep run() alive while idle
    std::jthread reactor([] { g_io.run(); });

    std::cout << "threads: " << load_settings() << '\n';

    guard.reset(); // let run() return so the jthread can join
}

