class Event {
 public:
  virtual ~Event() = default;
  virtual OrderResult ProcessNoAlloc() const = 0;
};

int64_t RunPreallocatedRing(
    const std::vector<std::unique_ptr<Event>>& ring,
    size_t n) {
  int64_t net_exposure = 0;
  for (size_t i = 0; i < n; ++i) {
    OrderResult res = ring[i & kMask]->ProcessNoAlloc();
    net_exposure += res.gross_notional - res.fee_ticks;
  }
  return net_exposure;
}
