      using handle = std::coroutine_handle<>;
      bool await_ready() const noexcept
      {
    trace("awaitable<",name,">::await_ready()");
    return m_promise.is_ready();
      }
      void await_suspend(handle next) const noexcept
      {
    trace("awaitable<",name,">::await_suspend()");
    m_promise.m_continuation = next;
      }
      T await_resume() const
      {
    trace("awaitable<",name,">::await_resume()");
    return m_promise.get();
      }
      promise<T,name>& m_promise;
    };
    trace("task<", name, ">::co_await()");
    return awaitable{ *m_promise };
  }
  promise<T,name>& get_promise() const { return *m_promise;}
  ~task() {
    trace("~task<",name,">()");
  }
  static task<T,name> make() {
    co_await std::suspend_always{};
  }
private:
  task(promise<T,name>* p) : m_promise(p) {
    trace("task<",name,">()");
  }
  friend class promise<T,name>;
  promise_ptr<T,name> m_promise;
};
