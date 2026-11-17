enum class EventKind : uint8_t {
  kLimit, kCancel, kModify, kFill, kPeg
};

struct FlatOrderEvent {
  int64_t price_ticks;
  int64_t qty;
  int32_t fee_bps;
  EventKind kind;
};

inline OrderResult ProcessFlat(const FlatOrderEvent& e) {
  return {e.price_ticks * e.qty, e.fee_bps};
}
