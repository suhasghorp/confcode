#include <memory>
#include <stdexcept>

struct Widget {
    void use() { /* may throw */ }
};

bool should_stop_early();

void legacy_style() {
    Widget* w = new Widget;
    if (should_stop_early()) return; // leak: forgot to delete on this path
    w->use();                        // leak if this throws
    delete w;
}

void modern_style() {
    auto w = std::make_unique<Widget>();
    if (should_stop_early()) return; // fine: destructor frees the Widget
    w->use();                        // fine even if this throws
}                                    // freed here on the normal path

bool should_stop_early() { return false; }

int main() {
    modern_style();
}

