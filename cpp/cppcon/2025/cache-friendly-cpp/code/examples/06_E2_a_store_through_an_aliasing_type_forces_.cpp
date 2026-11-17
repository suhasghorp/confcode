#include <cstdio>

template <typename T>
[[gnu::noinline]] int decrement_all(T* p, const int* count)
{
    int done = 0;
    for (int i = 0; i < *count; ++i) {   // is *count reloaded every iteration?
        p[i] = static_cast<T>(p[i] - 1);
        ++done;
    }
    return done;
}

int main()
{
    // Separate storage: an ordinary call.
    char8_t text[8] = {};
    int n = 8;
    int a = decrement_all(text, &n);

    // Overlapping storage, legal because unsigned char may alias any object:
    // the bytes being decremented are the bytes of count itself.
    int count = 2;
    int b = decrement_all(reinterpret_cast<unsigned char*>(&count), &count);

    std::printf("char8_t: %d iterations, unsigned char aliasing its own count: %d iteration\n", a, b);
}
