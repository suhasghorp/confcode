#include <sys/epoll.h>
#include <unistd.h>

#include <array>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>

class epoller {
public:
    using worker = std::function<void(std::span<char>)>;

    epoller() : epfd_(epoll_create1(0))
    {
        if (epfd_ < 0) throw std::runtime_error("epoll_create1");
    }
    ~epoller() { ::close(epfd_); }
    epoller(const epoller&) = delete;
    epoller& operator=(const epoller&) = delete;

    void add(int fd, worker w)
    {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;                       // returned to us in epoll_wait
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev) < 0) throw std::runtime_error("epoll_ctl");
        cbs_.emplace(fd, std::move(w));
    }

    void wait()
    {
        std::array<epoll_event, 16> events;
        int n = epoll_wait(epfd_, events.data(), static_cast<int>(events.size()), -1);
        for (int i = 0; i < n; ++i) {         // only ready descriptors are reported
            char buffer[1500];
            int fd = events[i].data.fd;
            auto len = ::read(fd, buffer, sizeof(buffer));
            if (len >= 0) cbs_[fd](std::span(buffer).first(static_cast<std::size_t>(len)));
        }
    }

private:
    int epfd_;
    std::map<int, worker> cbs_;
};

int main() { epoller e; (void)e; }
