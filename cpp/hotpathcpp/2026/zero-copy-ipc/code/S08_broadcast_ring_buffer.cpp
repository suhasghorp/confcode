// Hardware Cache Line Isolation (64 Bytes):
constexpr size_t kRingBufferCacheLineSize = 64;

struct alignas(kRingBufferCacheLineSize) ReaderSlot {
  std::atomic<bool> active{false};          // 1 byte
  std::atomic<uint64_t> cursor{0};          // 8 bytes
  std::atomic<int64_t> heartbeat_ts{0};     // 8 bytes
  std::atomic<pid_t> pid{0};                // 4 bytes
  std::atomic<uint32_t> generation_id{0};   // 4 bytes
  char padding[35];  // Explicit padding moat: completes 64-byte line!
};
static_assert(sizeof(ReaderSlot) == 64, "ReaderSlot must exactly fill 64 bytes");
