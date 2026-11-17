void Benchmark_Fast(int iters) {
  std::unordered_map<string, int> m;
  const std::pair<const string, int> p = {};
  while (iters--) m.insert(p);
}
