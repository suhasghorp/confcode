// BROKEN: do not do this. Compare with the talk's do_read in Listing 29.
void do_read() {
    socket_.async_read_some(
        asio::buffer(data_, max_length),
        [this](std::error_code ec, std::size_t length) {  // no `self`!
            // Nothing kept the session alive after start() returned:
            // the session was destroyed, its socket closed, and the read
            // cancelled. This handler now runs with a dangling `this`.
            // Any access to members (socket_, data_, do_write, ...) here
            // is undefined behaviour.
            if (!ec) { do_write(length); }
        });
}

