#include <iostream>

int a, b;

int compute(int n)
{
    int c = 0;                 // may never be stored to memory at all
    for (int i = 1; i <= n; ++i) c += i;   // may become n * (n + 1) / 2
    b = 2;                     // these two stores may be emitted in either order...
    a = 1;
    return c;
}

int main()
{
    int r = compute(100);
    std::cout << a << ' ' << b << ' ' << r << '\n';   // ...because only this output is observable
}
