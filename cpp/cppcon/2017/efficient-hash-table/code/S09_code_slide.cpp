void Benchmark_Slow(int iters) {
  std::unordered_map<string, int> m;
  std::pair<const string, int> p = {};
  while (iters--) m.insert(p);
}
