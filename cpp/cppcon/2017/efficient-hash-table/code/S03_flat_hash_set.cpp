iterator find(const K& key, size_t hash) const {
  size_t pos = H1(hash) % size_;
  while (true) {
    if (H2(hash) == ctrl_[pos] && key == slots_[pos])
      return iterator_at(pos);
    if (ctrl_[pos] == kEmpty) return end();
    pos = (pos + 1) % size_;
  }
}
