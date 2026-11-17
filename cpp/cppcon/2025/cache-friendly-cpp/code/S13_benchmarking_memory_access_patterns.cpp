for (std::size_t idx = 0; idx < data.size(); ++idx)
{
    benchmark::DoNotOptimize(dist(engine));
    benchmark::DoNotOptimize(++data[idx]);
}
