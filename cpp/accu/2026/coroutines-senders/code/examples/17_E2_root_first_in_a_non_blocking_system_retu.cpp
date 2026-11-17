#include <asio.hpp>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

// LEGACY continuation-passing code: left untouched by the migration.
void legacy_ticker(asio::steady_timer& t, int remaining) {
    if (remaining == 0) return;
    t.expires_after(100ms);
    t.async_wait([&t, remaining](const asio::error_code& ec) {
        if (ec) return;
        std::cout << "legacy tick " << remaining << '\n';
        legacy_ticker(t, remaining - 1);
    });
}

// Before the migration the root was:
//   void root(asio::steady_timer& t) { legacy_ticker(t, 3); return; }

// STEP 1: the root becomes a coroutine. The only change: return -> co_return.
asio::awaitable<void> root_step1(asio::steady_timer& legacy_timer) {
    legacy_ticker(legacy_timer, 3); // red code can call blue/callback code freely
    co_return;
}

// STEP 2 (later): one flow is migrated to co_await; the other is still legacy.
asio::awaitable<void> root_step2(asio::steady_timer& legacy_timer) {
    legacy_ticker(legacy_timer, 3);

    auto ex = co_await asio::this_coro::executor;
    asio::steady_timer t(ex); // lives in the coroutine frame: no shared_from_this needed
    for (int i = 3; i > 0; --i) {
        t.expires_after(150ms);
        co_await t.async_wait(asio::use_awaitable);
        std::cout << "coroutine tick " << i << '\n';
    }
}

int main() {
    asio::io_context io;
    asio::steady_timer legacy_timer(io);
    // Swap root_step1 for root_step2 as the migration progresses; main stays the same.
    asio::co_spawn(io, root_step2(legacy_timer), asio::detached);
    io.run();
}

