struct coro_deleter
{
  template <typename promise>
  void operator()(promise* p) const noexcept {
    using handle = std::coroutine_handle<promise>;
    handle::from_promise(*p).destroy();
  }
};

template <typename T>
using promise_ptr = std::unique_ptr<promise<T>,
                                    coro_deleter>;
