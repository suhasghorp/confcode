#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

class MagicBuffer {
    std::byte* addr_ = nullptr;
    std::size_t size_ = 0;
public:
    explicit MagicBuffer(std::size_t size) : size_(size) {
        if (size % static_cast<std::size_t>(::sysconf(_SC_PAGESIZE)) != 0)
            throw std::invalid_argument("size must be a multiple of the page size");
        int fd = ::memfd_create("magic", 0);
        if (fd < 0 || ::ftruncate(fd, static_cast<off_t>(size)) != 0) throw std::runtime_error("memfd");
        // 1. reserve 2 * size of address space with an anonymous mapping
        void* base = ::mmap(nullptr, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) { ::close(fd); throw std::runtime_error("reserve"); }
        addr_ = static_cast<std::byte*>(base);
        // 2. and 3. map the same file over both halves, at exact addresses
        for (std::byte* half : {addr_, addr_ + size}) {
            if (::mmap(half, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED) {
                ::munmap(addr_, 2 * size); ::close(fd);
                throw std::runtime_error("map half");
            }
        }
        ::close(fd);                         // the mappings keep the memory alive
    }
    ~MagicBuffer() { ::munmap(addr_, 2 * size_); }
    MagicBuffer(MagicBuffer const&) = delete;
    MagicBuffer& operator=(MagicBuffer const&) = delete;

    std::size_t size() const { return size_; }
    std::byte* at(std::size_t pos) { return addr_ + (pos & (size_ - 1)); }   // size_ bytes always follow
};

int main() {
    MagicBuffer ring(64 * 1024);
    std::printf("ring of %zu bytes at %p\n", ring.size(), static_cast<void*>(ring.at(0)));

    char const msg[] = "magic wrapped!";                // 14 characters
    std::size_t pos = ring.size() - 6;                  // only 6 bytes before the end
    std::memcpy(ring.at(pos), msg, sizeof msg - 1);     // one plain copy, no wrap logic

    std::printf("wrote across the end; start of ring now reads: \"%.8s\"\n",
                reinterpret_cast<char const*>(ring.at(0)));
    std::printf("the same bytes at ring[%zu]: \"%.14s\"\n", pos,
                reinterpret_cast<char const*>(ring.at(pos)));
}
