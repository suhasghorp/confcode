// Linux-specific: uses epoll(7) and POSIX file descriptors.
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <functional>
#include <unordered_map>

class Reactor {
public:
    Reactor() : epfd_(epoll_create1(0)) {}
    ~Reactor() { close(epfd_); }
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    void watch_readable(int fd, std::function<void()> on_ready) {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
        handlers_[fd] = std::move(on_ready);
    }

    void unwatch(int fd) {
        epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
        handlers_.erase(fd);
    }

    void run() {
        epoll_event events[16];
        while (!handlers_.empty()) {
            // The ONLY blocking call: wait until some fd is *ready*.
            int n = epoll_wait(epfd_, events, 16, -1);
            if (n < 0) {
                if (errno == EINTR) continue;
                std::perror("epoll_wait");
                return;
            }
            for (int i = 0; i < n; ++i) {
                auto it = handlers_.find(events[i].data.fd);
                if (it == handlers_.end()) continue;
                auto handler = it->second;  // copy: handler may unwatch itself
                handler();                  // dispatch: must not block!
            }
        }
    }

private:
    int epfd_;
    std::unordered_map<int, std::function<void()>> handlers_;
};

int main() {
    Reactor reactor;
    const int fd = STDIN_FILENO;  // try: echo hello | ./reactor
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    reactor.watch_readable(fd, [&] {
        char buf[256];
        for (;;) {
            ssize_t got = read(fd, buf, sizeof buf);  // non-blocking read
            if (got > 0) {
                std::printf("got %zd bytes\n", got);
            } else if (got == 0) {                     // EOF: this flow is done
                std::printf("EOF\n");
                reactor.unwatch(fd);
                return;
            } else {
                // EAGAIN: no more data right now; return to the loop.
                // (Other errors are treated the same way here for brevity.)
                return;
            }
        }
    });

    reactor.run();
}

