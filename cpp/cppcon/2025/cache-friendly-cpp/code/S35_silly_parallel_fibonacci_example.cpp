std::vector<unsigned> results(thread_count);
for (auto _ : state)
{
    pool.run([&](std::size_t thread_id) {
        results[thread_id] = 0;
        for (auto i = 0; i != 1024 / thread_count; ++i)
            accumulate_fib(results[thread_id], i % 20);
    });
    benchmark::DoNotOptimize(results);
}
