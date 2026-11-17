#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <stdexcept>
#include <utility>

class udp_socket {
public:
    udp_socket(const char* addr, std::uint16_t port)
        : fd_(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
    {
        if (fd_ < 0) throw std::runtime_error("socket");
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        if (::inet_pton(AF_INET, addr, &sa.sin_addr) != 1) throw std::runtime_error("inet_pton");
        if (::bind(fd_, reinterpret_cast<sockaddr*>(&sa), sizeof sa) < 0) throw std::runtime_error("bind");
    }
    udp_socket(udp_socket&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    udp_socket& operator=(udp_socket&&) = delete;
    ~udp_socket() { if (fd_ >= 0) ::close(fd_); }

    int fd() const noexcept { return fd_; }

private:
    int fd_;
};

int main()
{
    auto port4000 = udp_socket("127.0.0.1", 4000);
    return port4000.fd() >= 0 ? 0 : 1;
}
