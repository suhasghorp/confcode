// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33.
#include <atomic>
#include <mutex>
#include <string>

// Opt IN: a hand-rolled thread-safe type the default algorithm would reject
// (it has a mutable member). Specialising is our "unsafe impl Send + Sync".
class counter {
    mutable std::mutex m_;
    long value_ = 0;
public:
    void add(long n)   { std::scoped_lock lock(m_); value_ += n; }
    long get() const   { std::scoped_lock lock(m_); return value_; }
};
template <>
constexpr bool is_sendable_v<counter> = true;

// Opt OUT: a type that looks harmless member-wise but hands out
// mutable access to global state from its const member functions.
inline std::string global_log;
struct logger {
    int level = 0;
    std::string& log() const { return global_log; }
};
template <>
constexpr bool is_sendable_v<logger> = false;

static_assert(sendable<counter>);
static_assert(not sendable<logger>);

int main() {}
