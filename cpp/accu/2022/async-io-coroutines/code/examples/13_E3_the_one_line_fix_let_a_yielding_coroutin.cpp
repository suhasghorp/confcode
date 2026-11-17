// In promise<T> from Listing 23 and Listing 24: return suspend_never from yield_value.
// The downstream stage has already consumed the value when resume() returns.
template <typename U>
std::suspend_never yield_value(U&& u) {
  m_value.emplace(std::forward<U>(u));
  m_continuation.resume();
  return {};
}
