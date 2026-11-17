// Standalone Asio (https://think-async.com); with Boost, use <boost/asio.hpp>
// and the boost::asio namespace instead.
#include <asio.hpp>
#include <chrono>
#include <exception>
#include <iostream>

// Stand-in for an existing hand-written event loop.
struct LegacyReactor {
    int iterations = 0;
    void process_events() {
        ++iterations;
        // Real code: epoll_wait(fd, events, max, /*timeout=*/0),
        // then dispatch each event to its state machine.
    }
};

// New-style code: a coroutine running on the Asio io_context.
asio::awaitable<void> heartbeat(int count) {
    auto ex = co_await asio::this_coro::executor;
    asio::steady_timer timer(ex);
    for (int i = 0; i < count; ++i) {
        timer.expires_after(std::chrono::milliseconds(10));
        co_await timer.async_wait(asio::use_awaitable);  // suspension point
        std::cout << "heartbeat " << i << '\n';
    }
}

int main() {
    LegacyReactor legacy_reactor;
    asio::io_context io_ctx;

    // Prevent io_ctx from stopping itself when it is momentarily idle.
    auto guard = asio::make_work_guard(io_ctx);

    bool running = true;
    std::exception_ptr failure;

    asio::co_spawn(io_ctx, heartbeat(5),
                   [&](std::exception_ptr e) {
                       failure = e;      // keep errors, don't lose them
                       running = false;  // new-style work is done
                   });

    // We still own the loop; neither call below blocks.
    while (running) {
        legacy_reactor.process_events();
        io_ctx.poll();  // run every Asio handler that is ready right now
    }

    if (failure) std::rethrow_exception(failure);
    std::cout << "legacy reactor iterated "
              << legacy_reactor.iterations << " times\n";
}

