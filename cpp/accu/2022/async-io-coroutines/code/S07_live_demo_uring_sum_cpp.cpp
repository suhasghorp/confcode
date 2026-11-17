void ring::wait()
{

  auto work_cqe = [this](io_uring_cqe* entry) {
    auto w = static_cast<read_work*>(io_uring_cqe_get_data(entry));
    if (w->cb_({w->data_.data(), size_t(entry->res)})) {
      io_uring_cqe_seen(&uring_, entry);
      auto entry = io_uring_get_sqe(&uring_);
      io_uring_prep_read(entry, w->fd_, w->data_.data(), w->data_.size(), 0);
      io_uring_sqe_set_data(entry, w);
    }
    else {
      io_uring_cqe_seen(&uring_, entry);
      pending_.remove_if([&](const auto& e) { return &e == w; });
    }
  };
  io_uring_cqe* entry;
  io_uring_submit(&uring_);
  auto e = io_uring_wait_cqe(&uring_, &entry);
  if (e == 0) {
    work_cqe(entry);
    while (io_uring_peek_cqe(&uring_, &entry) == 0) {
      work_cqe(entry);
    }
  }
}
