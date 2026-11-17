template <typename BlockPolicyT>
struct TProcessIdLock
{
    using BlockPolicy = BlockPolicyT;

    TProcessIdLock() = default;

    explicit TProcessIdLock(std::in_place_t inplace);

    bool try_lock()
    {
        return pidlock_detail::try_lock_impl(
            data_.pid,
            ProcessId::current());
    }
