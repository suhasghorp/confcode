void erase(iterator it) {
  --size_;
  Group g{(it.ctrl_ - ctrl_) / 16 * 16 + ctrl_};
  *it.ctrl_ = g.MatchEmpty() ? kEmpty : kDeleted;
  it.slot_.~K()
}
