iterator find(const K& key, size_t hash) const {
  size_t group = H1(hash) % num_groups_;
  while (true) {
    Group g{ctrl_ + group * 16};
    for (int i : g.Match(H2(hash))) {
      if (key == slots_[group * 16 + i])
        return iterator_at(group * 16 + i);
    }
    if (g.MatchEmpty()) return end();
    group = (group + 1) % num_groups_;
  }
}
