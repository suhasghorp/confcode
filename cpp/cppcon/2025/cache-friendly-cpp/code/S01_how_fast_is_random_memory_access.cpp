void bm_memory_access(benchmark::State& state)
{
    std::default_random_engine engine;
    auto bytes = state.range(0) * 1024;
    std::vector<unsigned> data(bytes / sizeof(unsigned));
    stdr::generate(data, engine);
    std::uniform_int_distribution<std::size_t> dist(0, data.size() - 1);
    for (auto _ : state) {
        benchmark::DoNotOptimize(++data[dist(engine)]);
    }
    state.SetBytesProcessed(state.iterations() * data.size());
}
