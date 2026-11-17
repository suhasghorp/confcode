void Benchmark_Slow(int iters) {
  dense_hash_map<int, int, ScalarHash> m;
  std::uniform_int_distribution<int> dist;
  while (iters--)
    m.insert(std::make_pair(dist(rng) << 10, 0));
}
