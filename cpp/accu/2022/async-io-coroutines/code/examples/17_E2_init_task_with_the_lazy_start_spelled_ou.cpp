// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26.
#include <functional>

template <typename T, typename Pipeline>
std::move_only_function<bool(T)> make_pipeline_callback(Pipeline pipe)
{
  auto input = task<T>::make();
  auto& promise = input.get_promise();
  using coro_type = decltype(pipe(std::move(input)));

  return [&promise, pipe = std::move(pipe), input = std::move(input),
          coro = std::unique_ptr<coro_type>{}](T value) mutable {
    if (!coro) {
      // First call: we are now at our final address, so the pipeline's coroutines
      // may safely refer back to `pipe`'s captures.
      coro = std::make_unique<coro_type>(pipe(std::move(input)));
    }
    promise.yield_value(value);
    return true;
  };
}

int main()
{
  auto pipe = [](task<int> in) -> task<void> {
    for (;;) std::cout << co_await in << ' ';
  };
  auto cb = make_pipeline_callback<int>(pipe);
  for (int i = 1; i <= 3; ++i) cb(i);   // 1 2 3
  std::cout << '\n';
}
