class my_coro {
public:
   my_coro(int x_, int y_) : x(x_), y(y_) {}
   int get_result() const { return result; }
   void operator()(int a)
   {
      result += work(a, x, y);
   }
private:
   int x;
   int y;
   int result = 0;
};

auto coro_obj = impl(new my_coro(3, 8), source);
