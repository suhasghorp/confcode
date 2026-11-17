// Add after the primary promise<T> template from Listing 23 and Listing 24.
template <>
struct promise<void>
{
  task<void> get_return_object() noexcept { return {this}; }
  std::suspend_never initial_suspend() noexcept { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  void unhandled_exception() {}
  void return_void() {}
  std::coroutine_handle<> m_continuation;
};
