template <typename task_type, typename pipeline>
auto init_task(pipeline pipe)
{
   auto input = task<task_type>::make();
   auto& promise = input.get_promise();
   using coro_type = decltype(pipe(std::move(input)));
   return [&promise,
           pipe = std::move(pipe),
           input = std::move(input),
           coro = std::unique_ptr<coro_type>{}](task_type value) mutable
   {
      if (!coro) coro = std::make_unique<coro_type>(pipe(std::move(input)));
      promise.yield_value(value);
      return true;
   };
}
