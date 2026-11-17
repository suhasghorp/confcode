void increments_then_sum(const std::vector<unsigned>& data)
{
    auto count = 0;
    for (auto d : data)
    {
        if (d == 0) // rarely true
        {
            REPEAT64(benchmark::DoNotOptimize(++count));
        }
        count += d;
    }
    benchmark::DoNotOptimize(count);
}
