void sum_then_increments(const std::vector<unsigned>& data)
{
    auto count = 0;
    for (auto d : data)
    {
        count += d;
        if (d == 0) // rarely true
        {
            REPEAT64(benchmark::DoNotOptimize(++count));
        }
    }
    benchmark::DoNotOptimize(count);
}
