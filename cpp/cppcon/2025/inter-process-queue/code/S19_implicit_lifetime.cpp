struct X
{
    int a, b;
};

X * p = static_cast<X *>(std::malloc(sizeof(X)));
p->a = 1;
p->b = 2;
