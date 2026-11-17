// Uses the sendable machinery from Listing 27, Listing 29, Listing 32 and Listing 33.
#include <span>

struct point { int x; int y; };
struct holds_ptr { int* p; };
struct holds_cptr { const int* p; };
struct cached { mutable int cache; };

struct shape { virtual ~shape() = default; virtual double area() const = 0; };
struct square final : shape { double side; double area() const override { return side * side; } };
struct standalone final { int x; virtual int get() const { return x; } };
struct derived_point : point { int z; };

union number { int i; float f; };

static_assert(sendable<void(*)(int)>);
static_assert(sendable<const int*>);
static_assert(sendable<const int&>);
static_assert(not sendable<int*>);
static_assert(not sendable<int&>);
static_assert(not sendable<int point::*>);

static_assert(sendable<point>);
static_assert(sendable<derived_point>);         // bases are checked too
static_assert(not sendable<holds_ptr>);
static_assert(sendable<holds_cptr>);
static_assert(not sendable<cached>);            // mutable member
static_assert(not sendable<shape>);             // polymorphic, not final
static_assert(not sendable<square>);           // final, but its base `shape` is rejected
static_assert(sendable<standalone>);            // polymorphic, final, no polymorphic bases
static_assert(not sendable<number>);            // unions: is_class_type is false -> fallback
static_assert(not sendable<std::span<int>>);
static_assert(sendable<std::span<const int>>);

int main() {}
