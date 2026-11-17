s.reserve(1000);
s.insert(0);
for (int i = 0; i < 100000; ++i) {
  s.erase(i);

  // may rehash, `std::unordered_set` will not
  s.insert(i+1);
}
