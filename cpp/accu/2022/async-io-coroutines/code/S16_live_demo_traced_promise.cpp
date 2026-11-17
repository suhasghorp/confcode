struct promise
{
  promise() { trace("promise<",name,">()"); }
  ~promise() { trace("~promise<", name, ">()");}

  task<T,name> get_return_object() noexcept {
    trace("promise<",name,">::get_return_object()");
    return {this};
  }
  std::suspend_never initial_suspend() noexcept {
    trace("promise<",name,">::initial_suspend()");
    return {};
  }
  std::suspend_always final_suspend() noexcept {
    trace("promise<",name,">::final_suspend()");
    return {};
  }

  bool is_ready() const noexcept {
    bool has_value = m_value.has_value();
    trace("promise<",name,">::is_ready() -> ", has_value);
    return has_value;
  }

  T get() noexcept {
    auto t = std::move(m_value).value();
    trace("promise<",name,">::get() ->", t);
    m_value.reset();
    return t;
  }

  void unhandled_exception() {}
  template <typename U>
  std::suspend_always yield_value(U&& u)
  {
  trace("promise<",name,">::yield_value(",u,')');
  m_value.emplace(std::forward<U>(u));
  m_continuation.resume();
