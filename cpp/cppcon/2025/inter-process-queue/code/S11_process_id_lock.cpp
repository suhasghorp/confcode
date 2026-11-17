bool try_lock_impl(wjh::Atomic<ProcessId> & pid, ProcessId const & me)
{
    auto expected = PID::null();
    if (exchange(pid, expected, me)) return true;
    if (expected != me) {
        if (auto p = PID::maybe(expected.pid());
            not p || expected != *p)
        {
            exchange(pid, expected, PID::null());
            expected = PID::null();
            if (exchange(pid, expected, me)) return true;
        }
    }
    return false;
}
