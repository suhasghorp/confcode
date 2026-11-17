// ABA Prevention via 32-bit Generation ID Atomic CAS:
void BroadcastRingBuffer::Advance(BroadcastReader& reader, size_t frame_bytes) {
  int slot = reader.slot_index();
  if (slot >= 0) {
    auto& my_slot = header_->readers[slot];
    uint32_t expected_gen = reader.generation_id();

    // Verify reader's local epoch matches slot's epoch before updating cursor!
    if (my_slot.generation_id.compare_exchange_strong(
            expected_gen, expected_gen, std::memory_order_acquire)) {
      my_slot.cursor.store(new_cursor, std::memory_order_release);
      my_slot.heartbeat_ts.store(absl::ToUnixNanos(absl::Now()), std::memory_order_relaxed);
    } else {
      reader.set_slot_index(-1); // Evicted & replaced! Unbind cleanly.
    }
  }
}
