void Benchmark_Slow(int iters) {
  dense_hash_map<int, int> m;
  std::uniform_int_distribution<int> dist;
  while (iters--)
    m.insert(std::make_pair(dist(rng) << 10, 0));
}
