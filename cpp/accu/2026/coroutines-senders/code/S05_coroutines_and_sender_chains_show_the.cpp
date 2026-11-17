asio::awaitable<void> read(tcp::socket socket,
                   std::span<std::byte> span) {
  while (!span.empty()) {
    std::size_t bytes = co_await
                    socket.async_read_some(
                       asio::buffer(span),
                       asio::use_awaitable);
    if (bytes == 0) throw eof_error{};
    span = span.subspan(bytes);
  }
}
