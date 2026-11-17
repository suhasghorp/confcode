    void do_write(std::size_t length) {
        auto self(shared_from_this());
        asio::async_write(
            socket_, asio::buffer(data_,
                                  length),
            [this, self](std::error_code ec,
                        std::size_t /*length*/) {
                if (!ec) { do_read(); }
            });
    }

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};
