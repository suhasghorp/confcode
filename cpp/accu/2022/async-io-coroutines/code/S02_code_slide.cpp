class poller {
public:
  using worker = std::function<void(std::span<char> data)>;
  void add(int fd, worker w) {
    fds_.push_back({fd, POLLIN, 0});
    cbs_.emplace(fd, std::move(w));
  }
  void wait() {
    auto r = poll(fds_.data(), fds_.size(), -1);
    for (auto& e : fds_) {
      if (e.revents & POLLIN) {
        char buffer[1500];
        auto len = ::read(e.fd, buffer, sizeof(buffer));
        cbs_[e.fd](std::span(buffer).first(len));
      }
    }
  }
private:
  std::vector<pollfd> fds_;
  std::map<int, worker> cbs_;
};
