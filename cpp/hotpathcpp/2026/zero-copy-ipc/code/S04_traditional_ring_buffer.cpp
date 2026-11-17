// The Naive Wraparound Flaw: 2x Memcpy + Conditional Branch
void WriteMessage(const uint8_t* msg, size_t len) {
  if (head_ + len <= capacity_) {
    // Fast path: Contiguous write
    std::memcpy(buf_ + head_, msg, len);
  } else {
    // SLOW PATH: Split across buffer boundary
    size_t first_part = capacity_ - head_;
    std::memcpy(buf_ + head_, msg, first_part);            // Write tail
    std::memcpy(buf_, msg + first_part, len - first_part); // Write head
    // Result: Branch predictor penalty + fragmented memory access!
  }
  head_ = (head_ + len) % capacity_;
}
