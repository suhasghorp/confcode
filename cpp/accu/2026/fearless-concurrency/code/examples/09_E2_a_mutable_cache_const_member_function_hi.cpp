#include <cmath>
#include <optional>
#include <thread>

class circle {
    double radius_;
    mutable std::optional<double> area_cache_;   // logically const, physically mutable
public:
    explicit circle(double r) : radius_(r) {}

    double area() const
    {
        if (!area_cache_) {                         // two threads may both see "empty"...
            area_cache_ = M_PI * radius_ * radius_; // ...and both write: data race
        }
        return *area_cache_;
    }
};

int main()
{
    const circle c(2.0);   // even a born-const object is not safe to share!
    std::jthread a([&c] { (void)c.area(); });
    std::jthread b([&c] { (void)c.area(); });
}
