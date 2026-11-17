std::vector<std::size_t> indices(chunk_size);
std::iota(indices.begin(), indices.end(), 0);

std::uniform_int_distribution<std::size_t> dist(0, N - chunk_size - 1);
for (auto _ : state)
{
    state.PauseTiming();
    std::shuffle(indices.begin(), indices.end(), engine);
    state.ResumeTiming();

    auto start = dist(engine);
    for (auto i : indices)
        sum += data[start + i];
    benchmark::DoNotOptimize(sum);
}
