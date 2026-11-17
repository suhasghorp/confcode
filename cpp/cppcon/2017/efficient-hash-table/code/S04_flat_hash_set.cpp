BitMask<uint32_t> Match(h2_t hash) const {
  auto match = _mm_set1_epi8(hash);
  return BitMask<uint32_t>(
      _mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl)));
}
