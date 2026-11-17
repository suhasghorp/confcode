std::unique_lock<PidLock> producer_lock()
{
    return std::unique_lock<PidLock>(producer_lock_);
}

std::unique_lock<PidLock> producer_lock(std::try_to_lock_t arg)
{
    return std::unique_lock<PidLock>(producer_lock_, arg);
}

std::unique_lock<PidLock> try_producer_lock()
{
    return producer_lock(std::try_to_lock);
}
