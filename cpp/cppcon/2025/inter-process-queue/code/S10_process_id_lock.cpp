void unlock()
{
   auto me = ProcessId::current();
    data_.pid.compare_exchange_strong(
        me,
        ProcessId::null());
    data_.block_policy().notify_one();
}

ProcessId pid() const
{
    return data_.pid.load(std::memory_order_acquire);
}
