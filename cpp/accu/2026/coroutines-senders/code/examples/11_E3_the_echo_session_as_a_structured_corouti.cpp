// Requires standalone Asio (or Boost.Asio with the boost:: namespaces).
#include <asio.hpp>
#include <cstddef>
#include <exception>

using asio::ip::tcp;

// The socket and buffer live in the coroutine frame for exactly
// as long as the session runs. No heap-allocated session object,
// no reference counting.
asio::awaitable<void> session(tcp::socket socket) {
    char data[1024];
    try {
        for (;;) {
            std::size_t n = co_await socket.async_read_some(
                asio::buffer(data), asio::use_awaitable);
            co_await asio::async_write(
                socket, asio::buffer(data, n), asio::use_awaitable);
        }
    } catch (const std::exception&) {
        // EOF or I/O error: fall out of the loop. The frame (socket
        // included) is destroyed when the coroutine finishes.
    }
}

asio::awaitable<void> listener(tcp::acceptor acceptor) {
    for (;;) {
        tcp::socket socket = co_await acceptor.async_accept(asio::use_awaitable);
        // Still unstructured at this level: sessions are detached.
        // A fully structured server would spawn them into a scope
        // owned by the listener.
        asio::co_spawn(acceptor.get_executor(), session(std::move(socket)),
                       asio::detached);
    }
}

int main() {
    asio::io_context ctx;
    asio::co_spawn(ctx, listener(tcp::acceptor(ctx, {tcp::v4(), 5555})),
                   asio::detached);
    ctx.run();
}
