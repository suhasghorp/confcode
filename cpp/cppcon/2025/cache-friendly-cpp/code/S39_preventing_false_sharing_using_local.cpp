std::vector<unsigned> results(thread_count);
for (auto _ : state)
{
    pool.run([&](std::size_t thread_id) {
        auto local = 0u;
        for (auto i = 0; i != 1024 / thread_count; ++i)
            accumulate_fib(local, i % 20);
        results[thread_id] = local;
    });
    benchmark::DoNotOptimize(results);
}
