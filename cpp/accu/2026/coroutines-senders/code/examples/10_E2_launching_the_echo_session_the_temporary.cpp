#include <asio.hpp>   // standalone Asio; with Boost use <boost/asio.hpp>
#include <memory>

using asio::ip::tcp;

// Assumes `session` exactly as defined in the talk's slides (Listing 29 + Listing 30).

class server {
public:
    server(asio::io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](std::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<session>(std::move(socket))->start();
                    // The temporary shared_ptr is destroyed at the ';' above.
                    // From now on, ONLY the `self` captured inside the pending
                    // read callback keeps the session alive.
                }
                do_accept();  // accept the next client (again: CPS)
            });
    }

    tcp::acceptor acceptor_;
};

int main() {
    asio::io_context io;
    server s(io, 12345);
    io.run();   // the event loop: runs callbacks as I/O completes
}

