// Backpressure Scanning in Reserve():
uint64_t target_cursor = current_cursor + required_space;

// Check if target cursor laps the slowest registered consumer
if (target_cursor - cached_min_cursor_ > capacity_) {
  while (true) {
    uint64_t min_c = target_cursor;
    for (size_t i = 0; i < kMaxReaders; ++i) {
      if (header_->readers[i].active.load(std::memory_order_acquire)) {
        uint64_t rc = header_->readers[i].cursor.load(std::memory_order_acquire);
        if (rc < min_c) min_c = rc;
      }
    }
    cached_min_cursor_ = min_c;
    if (target_cursor - cached_min_cursor_ <= capacity_) break;
    _mm_pause(); // Yield hyperthread to readers
  }
}
