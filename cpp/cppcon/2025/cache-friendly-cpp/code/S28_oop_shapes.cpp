struct Shape
{
    virtual double area() const = 0;
};

struct Circle : Shape
{
    double area() const override;
};
struct Square : Shape
{
    double area() const override;
};
