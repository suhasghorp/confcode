void Benchmark_Fast(int iters) {
  std::unordered_map<string, int> m;
  string key = "abcdefghijklmnopqrst";
  while (iters--) m.find(key);
}
