// Enforcing Hardware Store Order with Release Fence:
void BroadcastRingBuffer::Commit() {
  auto* header_dest = reinterpret_cast<FrameHeader*>(
      data_buffer_ + current_reservation_.start_offset);
  uint32_t current_version = header_dest->version.load(std::memory_order_relaxed);
  header_dest->version.store(current_version + 1, std::memory_order_release);

  // CRITICAL: Ensure payload bytes are globally visible before cursor update!
  std::atomic_thread_fence(std::memory_order_release);

  header_->commit_cursor.store(current_reservation_.claimed_cursor,
                               std::memory_order_release);
}
