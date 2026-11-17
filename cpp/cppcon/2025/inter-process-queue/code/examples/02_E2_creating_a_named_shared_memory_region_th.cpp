#include <cstdio>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
    constexpr std::size_t size = 1 << 20;                  // 1 MiB
    int fd = ::shm_open("/demo_queue", O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) { std::perror("shm_open"); return 1; }     // EEXIST: someone created it
    if (::ftruncate(fd, size) != 0) { std::perror("ftruncate"); return 1; }

    void* base = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);                                           // the mapping stays valid
    if (base == MAP_FAILED) { std::perror("mmap"); return 1; }
    std::printf("mapped /demo_queue at %p\n", base);       // other processes get other addresses

    ::munmap(base, size);
    ::shm_unlink("/demo_queue");                           // remove the name when done
}
