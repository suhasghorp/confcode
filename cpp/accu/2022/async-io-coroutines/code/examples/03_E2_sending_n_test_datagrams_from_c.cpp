#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string_view>

int main()
{
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(4000);
    ::inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);

    std::string_view msg = "aa\n";
    for (int i = 0; i < 3; ++i) {
        ::sendto(fd, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
    }
    // and the "stop" packet
    to.sin_port = htons(4001);
    ::sendto(fd, "x", 1, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
    ::close(fd);
}
