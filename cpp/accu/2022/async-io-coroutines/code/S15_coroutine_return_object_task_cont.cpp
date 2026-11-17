template <typename T>
struct task
{
   auto operator co_await() const noexcept {
      struct awaitable {
         bool await_ready() const noexcept {
            return m_promise.is_ready();
         }
         void await_suspend(std::coroutine_handle<> next) const noexcept {
            m_promise.m_continuation = next;
         }
         T await_resume() const {
            return m_promise.get();
         }
         promise<T>& m_promise;
      };
      return awaitable{ *m_promise };
   }
};
