template <typename T>
struct promise
{
  task<T> get_return_object() noexcept;
  std::suspend_never initial_suspend() noexcept;
  std::suspend_always final_suspend() noexcept;

  bool is_ready() const noexcept;
  T get();

  void unhandled_exception();
  template <typename U>
  std::suspend_always yield_value(U&& u);
  void return_void();

  std::coroutine_handle<> m_continuation;
  std::optional<T> m_value;
};
