// Requires Boost.Asio (or standalone Asio with the namespace adjusted).
// awaitable_operators is an *experimental* Asio feature, not part of ISO C++.
#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <chrono>
#include <iostream>
#include <variant>

namespace asio = boost::asio;
using namespace asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

// Read some bytes, or give up after 2 seconds - whichever happens first.
asio::awaitable<void> read_with_timeout(asio::ip::tcp::socket& socket) {
    char buf[1024];
    asio::steady_timer timer(co_await asio::this_coro::executor, 2s);

    // awaitable<std::size_t> || awaitable<void>
    //   -> awaitable<std::variant<std::size_t, std::monostate>>
    auto result = co_await (
        socket.async_read_some(asio::buffer(buf), asio::use_awaitable) ||
        timer.async_wait(asio::use_awaitable));

    if (result.index() == 0)
        std::cout << "read " << std::get<0>(result) << " bytes\n";
    else
        std::cout << "timed out; the pending read was cancelled\n";
    // No manual cancellation, no shared_ptr: buf and timer simply live
    // in this coroutine's frame until both children are finished.
}

