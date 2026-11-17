enum Ctrl : ctrl_t {
  kEmpty = -128,   // 0b10000000
  kDeleted = -2,   // 0b11111110
  kSentinel = -1,  // 0b11111111
  // kFull =          0b0xxxxxxx
};

size_t H1(size_t hash) { return hash >> 7; }
ctrl_t H2(size_t hash) { return hash & 0x7F; }
