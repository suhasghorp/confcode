class poller {
public:
  using worker = std::function<void(std::span<char> data)>;
  void add(int fd, worker w) {
    fds_.push_back({fd, POLLIN, 0});
    cbs_.emplace(fd, std::move(w));
  }
  ...
};
