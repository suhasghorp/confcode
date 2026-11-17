struct session : public enable_shared_from_this<session> {
    session(tcp::socket socket) : socket_(std::move(socket)) {}
    void start() { do_read(); }
  private:
    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            asio::buffer(data_, max_length),
            [this, self](std::error_code ec,
                        std::size_t length) {
                if (!ec) { do_write(length); }
            });
        /* code here is probably a bug */
    }
