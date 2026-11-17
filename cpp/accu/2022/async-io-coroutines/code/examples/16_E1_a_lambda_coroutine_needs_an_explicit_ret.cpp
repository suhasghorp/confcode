// Uses task<T> and promise<T> from Listing 20, Listing 21, Listing 23, Listing 24 and Listing 26, with promise<void> added.
int main()
{
  int threshold = 5;

  // The lambda object holds `threshold`; the coroutine frame refers to it via the lambda.
  auto above = [threshold]<typename T>(task<T> in) -> task<T> {   // explicit return type
    for (;;) {
      auto v = co_await in;
      if (v > threshold) co_yield v;
    }
  };

  auto source = task<int>::make();
  auto& p = source.get_promise();
  auto stage = above(std::move(source));    // `above` must outlive `stage`'s coroutine
  auto print = [](task<int> in) -> task<void> {
    for (;;) std::cout << co_await in << ' ';
  }(std::move(stage));                      // captureless: nothing to dangle

  for (int i = 0; i < 10; ++i) p.yield_value(i);   // 6 7 8 9
  std::cout << '\n';
}
