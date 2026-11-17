using Value = std::conditional_t<
    std::atomic<__uint128_t>::is_always_lock_free,
    __uint128_t,
    std::uint64_t>;
Value value_;
