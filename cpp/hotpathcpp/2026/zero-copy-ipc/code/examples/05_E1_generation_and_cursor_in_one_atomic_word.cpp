// Closing the window between "check generation" and "store cursor":
// keep both in one 64-bit word and update them with a single CAS.
#include <atomic>
#include <cstdint>
#include <cstdio>

class SlotState {
 public:
  static constexpr int kCursorBits = 48;                    // 256 TiB of stream offset
  static constexpr uint64_t kCursorMask = (uint64_t{1} << kCursorBits) - 1;

  static uint64_t Pack(uint32_t gen, uint64_t cursor) {
    return (uint64_t{gen & 0xFFFF} << kCursorBits) | (cursor & kCursorMask);
  }
  static uint32_t Gen(uint64_t s) { return static_cast<uint32_t>(s >> kCursorBits); }
  static uint64_t Cursor(uint64_t s) { return s & kCursorMask; }

  // Consumer: publish a new cursor only if the slot still belongs to `my_gen`.
  bool Advance(uint32_t my_gen, uint64_t new_cursor) {
    uint64_t cur = state_.load(std::memory_order_acquire);
    do {
      if (Gen(cur) != (my_gen & 0xFFFF)) return false;     // evicted or replaced
    } while (!state_.compare_exchange_weak(cur, Pack(my_gen, new_cursor),
                                           std::memory_order_release,
                                           std::memory_order_acquire));
    return true;
  }

  // Producer or a new consumer: take the slot over by bumping the generation.
  uint32_t Claim(uint64_t start_cursor) {
    uint64_t cur = state_.load(std::memory_order_acquire);
    uint32_t next;
    do {
      next = (Gen(cur) + 1) & 0xFFFF;
    } while (!state_.compare_exchange_weak(cur, Pack(next, start_cursor),
                                           std::memory_order_acq_rel));
    return next;
  }

  uint64_t cursor() const { return Cursor(state_.load(std::memory_order_acquire)); }

 private:
  std::atomic<uint64_t> state_{0};
};

int main() {
  SlotState slot;
  uint32_t a = slot.Claim(0);           // consumer A registers
  slot.Advance(a, 4096);
  uint32_t b = slot.Claim(8192);        // A is evicted; B takes the slot
  bool zombie = slot.Advance(a, 5000);  // A wakes up and tries to publish
  bool live = slot.Advance(b, 9000);
  std::printf("gen A=%u B=%u | zombie update accepted: %s | B update accepted: %s | cursor %llu\n",
              a, b, zombie ? "yes" : "no", live ? "yes" : "no",
              static_cast<unsigned long long>(slot.cursor()));
}
