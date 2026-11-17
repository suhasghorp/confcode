size_t find_insert_position(size_t group_pos, size_t hash) {
#if !defined(NDEBUG)
  // To force small tables to have random entries, insert into
  // either the front or back of the group.
  if (H1(hash, ctrl_) % 13 > 6)
    return group_pos + mask.HighestBitSet();
  else
    return group_pos + mask.LowestBitSet();
#else
  return group_pos + mask.LowestBitSet();
#endif
}
