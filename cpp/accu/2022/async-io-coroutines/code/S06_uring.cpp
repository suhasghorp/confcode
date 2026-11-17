struct ring::read_work {
  work cb_;
  int fd_;
  std::array<char, 1500> buffer_;
};

void ring::add(int fd, work w)
{
  auto& work = pending_.emplace_back();
  work.cb_ = std::move(w);
  work.fd_ = fd;
  auto entry = io_uring_get_sqe(&uring_);
  io_uring_prep_read(entry, fd,
                     work.buffer_.data(),
                     work.buffer_.size(),
                     0);
  io_uring_sqe_set_data(entry, &work);
}
