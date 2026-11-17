async_read(socket, data) → awaitable<tuple1>;

timer::async_wait() → awaitable<tuple2>;

operator||(awaitable<tuple1>, awaitable<tuple2>)
    → awaitable<variant<tuple1, tuple2>>;


auto result = co_await (async_read(S, data) || T.async_wait());
