#include <chrono>
#include <cstdio>
#include <memory>
#include <numbers>
#include <random>
#include <variant>
#include <vector>

namespace oop {
struct Shape { virtual ~Shape() = default; virtual double area() const = 0; };
struct Circle : Shape { double r; explicit Circle(double r) : r(r) {} double area() const override { return std::numbers::pi * r * r; } };
struct Square : Shape { double s; explicit Square(double s) : s(s) {} double area() const override { return s * s; } };

double total_area(const std::vector<std::unique_ptr<Shape>>& shapes)
{
    double total = 0.0;
    for (const auto& shape : shapes) total += shape->area();
    return total;
}
}

namespace dod {
struct Circle { double r; };
struct Square { double s; };

template <typename... F> struct overloaded : F... { using F::operator()...; };

double total_area(const std::vector<std::variant<Circle, Square>>& shapes)
{
    double total = 0.0;
    for (const auto& shape : shapes)
        total += std::visit(overloaded{
            [](const Circle& c) { return std::numbers::pi * c.r * c.r; },
            [](const Square& q) { return q.s * q.s; }}, shape);
    return total;
}

double total_area(const std::vector<Circle>& circles, const std::vector<Square>& squares)
{
    double total = 0.0;
    for (const Circle& c : circles) total += std::numbers::pi * c.r * c.r;   // no branch
    for (const Square& q : squares) total += q.s * q.s;                      // no branch
    return total;
}
}

template <typename F>
void measure(const char* name, F f)
{
    auto start = std::chrono::steady_clock::now();
    double area = f();
    auto stop = std::chrono::steady_clock::now();
    std::printf("%-22s %.6e  %7.2f ms\n", name, area,
                std::chrono::duration<double, std::milli>(stop - start).count());
}

int main()
{
    std::mt19937 engine(13);
    std::uniform_real_distribution<double> size(0.5, 2.0);
    std::bernoulli_distribution is_circle(0.5);

    std::vector<std::unique_ptr<oop::Shape>> boxed;
    std::vector<std::variant<dod::Circle, dod::Square>> tagged;
    std::vector<dod::Circle> circles;
    std::vector<dod::Square> squares;
    for (int i = 0; i < 1'000'000; ++i) {
        double x = size(engine);
        if (is_circle(engine)) {
            boxed.push_back(std::make_unique<oop::Circle>(x));
            tagged.emplace_back(dod::Circle{x});
            circles.push_back({x});
        } else {
            boxed.push_back(std::make_unique<oop::Square>(x));
            tagged.emplace_back(dod::Square{x});
            squares.push_back({x});
        }
    }

    measure("unique_ptr<Shape>", [&] { return oop::total_area(boxed); });
    measure("variant<Circle,Square>", [&] { return dod::total_area(tagged); });
    measure("two vectors", [&] { return dod::total_area(circles, squares); });
    std::printf("sizeof(variant) = %zu, sizeof(Circle) = %zu\n",
                sizeof(std::variant<dod::Circle, dod::Square>), sizeof(dod::Circle));
}
