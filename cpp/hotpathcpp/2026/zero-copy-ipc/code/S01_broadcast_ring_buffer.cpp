// Industrial Single-Producer Multi-Consumer (SPMC) Broadcast
// Lock-free shared memory fabric eliminating Unix socket overhead.

class BroadcastRingBuffer {
 public:
  static absl::StatusOr<std::unique_ptr<BroadcastRingBuffer>>
  Create(const std::string& name, size_t capacity);

  // Producer reserves contiguous zero-copy slot directly in shared RAM
  absl::StatusOr<uint8_t*> Reserve(uint32_t payload_size, uint32_t type_id);
  void Commit();

  // Wait-free consumer reads with direct pointer span return
  absl::StatusOr<absl::Span<const uint8_t>> PollZeroCopy(
      BroadcastReader& reader, FrameHeader& header_out);
};
