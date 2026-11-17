template <typename Fn, typename... Args>
    requires sendable<std::decay_t<Fn>> && (sendable<std::decay_t<Args>> && ...)
[[nodiscard]]
auto spawn_thread(Fn&& fn, Args&&... args) -> std::jthread
{
    return std::jthread(std::forward<Fn>(fn), std::forward<Args>(args)...);
}
