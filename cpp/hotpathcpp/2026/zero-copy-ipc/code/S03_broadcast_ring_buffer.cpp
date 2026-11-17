// Dual Shared Memory File Partitioning:
absl::StatusOr<std::unique_ptr<BroadcastRingBuffer>>
BroadcastRingBuffer::Attach(const std::string& name, bool writable) {
  // 1. Metadata Segment: MUST be read-write for consumer cursor registration
  auto meta_or = MemoryMap::Open(absl::StrCat(name, "_meta"), kHeaderSize,
                                 /*read_only=*/false);
  if (!meta_or.ok()) return meta_or.status();

  // 2. Data Segment: Consumer mapped strictly READ-ONLY to prevent corruption
  auto data_or = MemoryMap::Open(absl::StrCat(name, "_data"), capacity,
                                 /*read_only=*/!writable, /*use_magic_mirror=*/true);
  if (!data_or.ok()) return data_or.status();

  return std::unique_ptr<BroadcastRingBuffer>(
      new BroadcastRingBuffer(std::move(*met
