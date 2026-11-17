#include <cstddef>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

struct Shared {
    int  value;
    int* pointer;            // an absolute address: only valid in one view
    std::size_t offset;      // relative to the start of the region: valid everywhere
};

int main() {
    int fd = ::memfd_create("demo", 0);                 // an anonymous file (Linux)
    if (fd < 0 || ::ftruncate(fd, 4096) != 0) { std::perror("memfd"); return 1; }
    auto* a = static_cast<Shared*>(::mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    auto* b = static_cast<Shared*>(::mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    std::printf("view A at %p, view B at %p\n", static_cast<void*>(a), static_cast<void*>(b));

    a->value = 42;
    a->pointer = &a->value;                             // points into view A
    a->offset = offsetof(Shared, value);
    std::printf("B reads value %d\n", b->value);        // 42: the bytes are shared
    std::printf("B's pointer still points into A: %s\n",
                b->pointer == &a->value ? "yes" : "no");
    auto* via_offset = reinterpret_cast<int*>(reinterpret_cast<char*>(b) + b->offset);
    std::printf("B via offset: %p (inside view B), value %d\n",
                static_cast<void*>(via_offset), *via_offset);
}
