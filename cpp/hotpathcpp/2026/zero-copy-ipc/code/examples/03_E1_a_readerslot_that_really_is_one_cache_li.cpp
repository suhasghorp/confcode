#include <sys/types.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

constexpr size_t kRingBufferCacheLineSize = 64;

// Largest fields first: no hidden padding between members.
struct alignas(kRingBufferCacheLineSize) ReaderSlot {
  std::atomic<uint64_t> cursor{0};         // offset  0
  std::atomic<int64_t> heartbeat_ts{0};    // offset  8
  std::atomic<pid_t> pid{0};               // offset 16
  std::atomic<uint32_t> generation_id{0};  // offset 20
  std::atomic<bool> active{false};         // offset 24
  // No explicit padding needed: alignas(64) rounds sizeof up to 64.
};

static_assert(sizeof(ReaderSlot) == kRingBufferCacheLineSize);
static_assert(alignof(ReaderSlot) == kRingBufferCacheLineSize);
static_assert(offsetof(ReaderSlot, active) == 24);  // 25 bytes used, 39 bytes of tail padding

int main() {
  ReaderSlot roster[16];
  std::printf("sizeof %zu, slot 1 starts %td bytes after slot 0\n", sizeof(ReaderSlot),
              reinterpret_cast<char*>(&roster[1]) - reinterpret_cast<char*>(&roster[0]));
}
