#include <liburing.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdio>

int main()
{
    // A UDP socket on 127.0.0.1:4000.
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(4000);
    ::inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    ::bind(fd, reinterpret_cast<sockaddr*>(&sa), sizeof sa);

    io_uring ring;
    if (io_uring_queue_init(8, &ring, 0) < 0) return 1;      // SQ/CQ with 8 entries

    std::array<char, 1500> buffer;                         // must outlive the request
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fd, buffer.data(), buffer.size(), 0);
    io_uring_sqe_set_data(sqe, buffer.data());             // our 64 bits of context

    io_uring_submit(&ring);                                // one system call

    io_uring_cqe* cqe = nullptr;
    if (io_uring_wait_cqe(&ring, &cqe) == 0) {             // sleeps until completion
        if (cqe->res >= 0) {
            std::printf("read %d bytes: %.*s", cqe->res, cqe->res,
                        static_cast<char*>(io_uring_cqe_get_data(cqe)));
        }
        io_uring_cqe_seen(&ring, cqe);                     // free the CQ slot
    }

    io_uring_queue_exit(&ring);
    ::close(fd);
}
