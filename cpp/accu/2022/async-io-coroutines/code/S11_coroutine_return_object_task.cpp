template <typename T>
struct task
{
  using promise_type = promise<T>;

  auto operator co_await() const noexcept;
private:
  task(promise<T>* p) : m_promise(p) { }
  friend class promise<T>;
  promise_ptr<T> m_promise;
};
