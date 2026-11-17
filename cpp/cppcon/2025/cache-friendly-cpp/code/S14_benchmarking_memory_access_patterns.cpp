for (std::size_t idx = data.size(); idx > 0; --idx)
{
    benchmark::DoNotOptimize(dist(engine));
    benchmark::DoNotOptimize(++data[idx - 1]);
}
