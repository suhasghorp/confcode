auto filter_in = [](auto predicate)
{
   return [predicate]<typename T>(task<T> in) -> task<T>
   {
      for (;;) {
         auto v = co_await in;
         if (predicate(v)) {
            co_yield v;
         }
      }
   };
};
