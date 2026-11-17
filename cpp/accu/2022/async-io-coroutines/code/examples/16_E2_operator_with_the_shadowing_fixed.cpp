// The slide's operator| with the inner template parameter renamed from T to V.
template <typename T>
struct coro_stage : T {
     coro_stage(T t) : T(std::move(t)) {}
};

template <typename T>
coro_stage(T) -> coro_stage<T>;

template <typename T, typename U>
auto operator|(coro_stage<T> lh, coro_stage<U> rh) {
   return coro_stage{
      [lh = std::move(lh), rh = std::move(rh)]
      <typename V>(task<V> in)              // was <typename T>: shadows operator|'s T
      {
         return rh(lh(std::move(in)));
      }
   };
}
