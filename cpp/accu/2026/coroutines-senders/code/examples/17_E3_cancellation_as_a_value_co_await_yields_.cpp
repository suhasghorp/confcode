#include <asio.hpp>
#include <chrono>
#include <expected>
#include <iostream>
#include <system_error>

using namespace std::chrono_literals;

struct cancelled_t {}; // "nobody needs this result any more"

// Wraps an Asio wait so that cancellation is a value, not an exception.
asio::awaitable<std::expected<void, cancelled_t>> wait_for(asio::steady_timer& t) {
    auto [ec] = co_await t.async_wait(asio::as_tuple(asio::use_awaitable));
    if (ec == asio::error::operation_aborted)
        co_return std::unexpected(cancelled_t{});
    if (ec)
        throw std::system_error(ec); // real errors still propagate as errors
    co_return std::expected<void, cancelled_t>{};
}

// Structured side: cancellation is just another branch.
asio::awaitable<void> consumer(asio::steady_timer& t) {
    auto r = co_await wait_for(t);
    if (!r) {
        std::cout << "cancelled - cleaning up and finishing normally\n";
        co_return;
    }
    std::cout << "event arrived\n";
}

int main() {
    asio::io_context io;
    asio::steady_timer event_timer(io, 10s); // the event we might wait a long time for

    asio::co_spawn(io, consumer(event_timer), asio::detached);

    // LEGACY side: it knows nothing about coroutine handles.
    // When it decides the result is no longer needed, it just requests cancellation.
    asio::steady_timer legacy_deadline(io, 100ms);
    legacy_deadline.async_wait([&](const asio::error_code&) { event_timer.cancel(); });

    io.run();
}

