static_assert(sizeof(Order) == 32);

class MatchingEngine {
 public:
  int64_t OnPacket(const FlatOrderEvent& ev) noexcept {
    if (__builtin_expect(ev.kind == kMarketHalt, 0)) {
      return ExecuteCircuitBreaker(ev);
    }
    Order* slot = slab_pool_.Allocate(ev);
    return slot->price * slot->quantity;
  }
 private:
  SlabPool<Order> slab_pool_;
};
