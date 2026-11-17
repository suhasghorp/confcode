template <size_t N>
struct str {
  constexpr str(const char* p) noexcept {
    std::copy_n(p, N, cstr);
  }
  friend std::ostream& operator<<(std::ostream& os, const str& s) {
    return os << s.cstr;
  }
  char cstr[N];
};

template <size_t N>
str(const char (&)[N]) -> str<N>;
