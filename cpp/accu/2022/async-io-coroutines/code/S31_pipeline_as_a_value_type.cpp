template <typename T, typename P>
task<T> filter_in(P predicate, task<T> in)
{
  for (;;) {
    auto v = co_await in;
    if (predicate(v)) {
      co_yield v;
    }
  }
}
