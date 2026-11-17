#include <string>
#include <type_traits>

struct Point { int x, y; };                          // aggregate
struct WithCtor { WithCtor(int); int v; };           // user constructor, but trivial copy/move ctor
struct NoTrivialCtor {                               // user-provided copy and move: none is trivial
    NoTrivialCtor(int);
    NoTrivialCtor(NoTrivialCtor const&);
    NoTrivialCtor(NoTrivialCtor&&);
};
struct WithDtor { ~WithDtor(); int v; };             // non-trivial destructor
struct Holder { std::string s; };                    // aggregate (any aggregate qualifies)

static_assert(std::is_implicit_lifetime_v<int>);
static_assert(std::is_implicit_lifetime_v<double*>);
static_assert(std::is_implicit_lifetime_v<Point>);
static_assert(std::is_implicit_lifetime_v<WithCtor>);        // its implicit copy ctor is trivial
static_assert(!std::is_implicit_lifetime_v<NoTrivialCtor>);
static_assert(!std::is_implicit_lifetime_v<WithDtor>);
static_assert(std::is_implicit_lifetime_v<Holder>);          // but its std::string member is not created!
static_assert(std::is_implicit_lifetime_v<std::string[4]>);  // arrays always are
static_assert(!std::is_implicit_lifetime_v<std::string>);

int main() {}
