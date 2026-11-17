bool demo() {
  flat_hash_set<int> i1 = { 0, 1 };
  flat_hash_set<int> i2 = { 0, 1 };
  return *i1.begin() == *i2.begin();
}
