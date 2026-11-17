void lock()
{
    auto const me = ProcessId::current();
    for (int i = 0; i < 10; ++i) {
        if (pidlock_detail::try_lock_impl(data_.pid, me)) {
            return;
        }
        std::this_thread::yield();
    }
    data_.block_policy().wait([&] {
        return pidlock_detail::try_lock_impl(data_.pid, me);
    });
}
