template <typename Fn1, typename Fn2>
    requires sendable<std::decay_t<Fn1>> &&
             sendable<std::decay_t<Fn2>>
void fork(Fn1&& fn1, Fn2&& fn2)
{
    auto thread = spawn_thread(std::forward<Fn2>(fn2));
    auto{fn1}();
    thread.join();
}
