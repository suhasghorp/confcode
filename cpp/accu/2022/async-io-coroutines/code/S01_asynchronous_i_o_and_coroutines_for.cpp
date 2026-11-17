#include <coroutine>
...
x = co_await source;
co_yield computation(x);
