auto print_all = [](std::ostream& dest)
{
   return [&dest]<typename T>(task<T> in) -> task<void>
   {
      for (;;)
      {
         auto v = co_await in;
         dest << v << '\n' << std::flush;
      }
   };
};
