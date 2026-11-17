size_t HashSeed(const ctrl_t* ctrl) {
#if !defined(NDEBUG)
  static const size_t kSeed = rand();
  return kSeed ^
#else
  return
#endif
      (reinterpret_cast<uintptr_t>(ctrl) >> 12);
}

size_t H1(size_t hash) {
  return (hash >> 7) ^ HashSeed(ctrl_);
}
