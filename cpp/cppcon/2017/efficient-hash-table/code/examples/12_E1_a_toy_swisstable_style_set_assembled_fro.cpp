// Builds on Listing 10, Listing 14, Listing 16 and Listing 17 (Ctrl, H1/H2, Match, find, erase); glue code added.
#include <emmintrin.h>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>

using ctrl_t = signed char;
using h2_t = std::uint8_t;
enum Ctrl : ctrl_t { kEmpty = -128, kDeleted = -2, kSentinel = -1 };
std::size_t H1(std::size_t hash) { return hash >> 7; }
ctrl_t H2(std::size_t hash) { return hash & 0x7F; }

template <typename T> struct BitMask {
  T mask;
  explicit BitMask(T m) : mask(m) {}
  struct iterator { T m; int operator*() const { return std::countr_zero(m); }
                    iterator& operator++() { m &= m - 1; return *this; }
                    bool operator!=(const iterator& o) const { return m != o.m; } };
  iterator begin() const { return {mask}; }
  iterator end() const { return {0}; }
  explicit operator bool() const { return mask != 0; }
};

struct Group {
  __m128i ctrl;
  explicit Group(const ctrl_t* p) : ctrl(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p))) {}
  BitMask<std::uint32_t> Match(h2_t hash) const {
    auto match = _mm_set1_epi8(static_cast<char>(hash));
    return BitMask<std::uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(match, ctrl)));
  }
  BitMask<std::uint32_t> MatchEmpty() const { return Match(static_cast<h2_t>(kEmpty)); }
  BitMask<std::uint32_t> MatchEmptyOrDeleted() const {    // special bytes are < kSentinel
    return BitMask<std::uint32_t>(_mm_movemask_epi8(_mm_cmpgt_epi8(_mm_set1_epi8(kSentinel), ctrl)));
  }
};

template <typename K> class toy_flat_set {
public:
  explicit toy_flat_set(std::size_t groups) : num_groups_(groups), ctrl_(groups * 16, kEmpty), slots_(groups * 16) {}
  struct iterator { ctrl_t* ctrl_; K* slot_; bool operator==(const iterator&) const = default; };
  iterator end() { return {nullptr, nullptr}; }
  iterator iterator_at(std::size_t i) { return {&ctrl_[i], &slots_[i]}; }
  static std::size_t hash_of(const K& k) { return std::hash<K>{}(k) * 0x9E3779B97F4A7C15ull; }

  iterator find(const K& key) { return find(key, hash_of(key)); }
  iterator find(const K& key, std::size_t hash) {
    std::size_t group = H1(hash) % num_groups_;
    while (true) {
      Group g{ctrl_.data() + group * 16};
      for (int i : g.Match(H2(hash))) {
        if (key == slots_[group * 16 + i])
          return iterator_at(group * 16 + i);
      }
      if (g.MatchEmpty()) return end();
      group = (group + 1) % num_groups_;
    }
  }

  void insert(const K& key) {                                 // glue: no growth
    std::size_t hash = hash_of(key);
    if (find(key, hash) != end()) return;
    for (std::size_t group = H1(hash) % num_groups_;; group = (group + 1) % num_groups_) {
      if (auto free = Group{ctrl_.data() + group * 16}.MatchEmptyOrDeleted()) {
        std::size_t pos = group * 16 + *free.begin();
        ctrl_[pos] = H2(hash); slots_[pos] = key; ++size_;
        return;
      }
    }
  }

  void erase(iterator it) {
    --size_;
    Group g{(it.ctrl_ - ctrl_.data()) / 16 * 16 + ctrl_.data()};
    *it.ctrl_ = g.MatchEmpty() ? kEmpty : kDeleted;
    it.slot_->~K();
  }

  std::size_t size() const { return size_; }
  std::size_t tombstones() const { std::size_t n = 0; for (auto c : ctrl_) n += (c == kDeleted); return n; }

private:
  std::size_t num_groups_, size_ = 0;
  std::vector<ctrl_t> ctrl_;
  std::vector<K> slots_;
};

int main()
{
  toy_flat_set<int> s(8);                                     // 8 groups = 128 positions
  for (int i = 0; i < 60; ++i) s.insert(i * 7);
  int found = 0;
  for (int i = 0; i < 420; ++i) found += (s.find(i) != s.end());
  for (int i = 0; i < 30; ++i) s.erase(s.find(i * 7));
  std::printf("found %d, size %zu, tombstones %zu\n", found, s.size(), s.tombstones());
}
