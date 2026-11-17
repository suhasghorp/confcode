#include <asio.hpp>
#include <cstddef>
#include <span>

// Build with -DASYNC_COLOR=1 for the coroutine version, 0 for the blocking one.
#ifndef ASYNC_COLOR
#define ASYNC_COLOR 1
#endif

#if ASYNC_COLOR
  #define TASK(T)            asio::awaitable<T>
  #define CO_AWAIT           co_await
  #define CO_RETURN          co_return
  #define READ_SOME(sock, b) (sock).async_read_some((b), asio::use_awaitable)
#else
  #define TASK(T)            T
  #define CO_AWAIT           /* nothing */
  #define CO_RETURN          return
  #define READ_SOME(sock, b) (sock).read_some(b)
#endif

// Fill the whole span from the socket (same task as section 15).
// In both colors, Asio reports EOF by throwing asio::error::eof.
TASK(std::size_t) read_exactly(asio::ip::tcp::socket& sock, std::span<std::byte> buf) {
    std::size_t total = 0;
    while (!buf.empty()) {
        std::size_t n = CO_AWAIT READ_SOME(sock, asio::buffer(buf.data(), buf.size()));
        total += n;
        buf = buf.subspan(n);
    }
    CO_RETURN total;
}

// Compare the two builds' outputs in your tests to check functional equivalence.
// Caveat: this does not test interleavings; in the async build, other tasks
// may run at every CO_AWAIT.

