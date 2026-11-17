#include <vector>

// A handle to someone else's vector that tries to be deep-const:
class weird_handle {
    std::vector<int>* target_;
public:
    explicit weird_handle(std::vector<int>& v) : target_(&v) {}

    void push(int x) { target_->push_back(x); }                // non-const: mutates far-away data
    int size() const { return static_cast<int>(target_->size()); }  // const: read-only
};

// Sharing a const weird_handle& between threads only allows size(): "Sync".
// But a *copy* sent by value lets the receiver push() into the original vector
// while others read it: not "Send". Copying a handle that claims deep const is surprising.

int main()
{
    std::vector<int> v;
    weird_handle h(v);
    weird_handle copy = h;   // shallow copy...
    copy.push(1);            // ...that mutates the original
}
