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
  std::suspend_always yield_value(U&& u){
    m_value.emplace(std::forward<U>(u));
    m_continuation.resume();
    return {};
  }
};
