std::vector<T> local_results(pool.thread_count());
pool.run([&](std::size_t thread_idx) {
    local_results[thread_idx] = fold(work.slice(thread_idx));
});

T global_result = fold(local_results);
