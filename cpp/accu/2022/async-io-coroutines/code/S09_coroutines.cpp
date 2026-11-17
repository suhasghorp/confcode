coroutine_type
my_coro(int x, int y, coro_src& src)
{
   int result = 0;
   while (int a = co_await src)
   {
      result += work(a,x,y);
   }
   co_return result;
}

auto coro_obj = my_coro(3, 8, source);
