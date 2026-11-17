#include <atomic>
#include <cstddef>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

struct SharedHeader {                     // lives at the start of the shared region
    std::size_t buffer_offset;            // where the slots start, relative to the region
    std::size_t capacity;                 // number of int slots, a power of two
    alignas(64) std::size_t write_pos;    // accessed only through std::atomic_ref
    alignas(64) std::size_t read_pos;
};

struct LocalView {                        // per-process state, never in shared memory
    SharedHeader* header;
    int* slots;
    explicit LocalView(void* region)
        : header(static_cast<SharedHeader*>(region)),
          slots(reinterpret_cast<int*>(static_cast<char*>(region) + header->buffer_offset)) {}
};

int main() {
    constexpr std::size_t bytes = 4096;
    int fd = ::memfd_create("queue", 0);
    if (fd < 0 || ::ftruncate(fd, bytes) != 0) return 1;
    void* producer_map = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    void* consumer_map = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    std::printf("producer maps region at %p, consumer at %p\n", producer_map, consumer_map);

    auto* h = static_cast<SharedHeader*>(producer_map);  // the creator fills in the header
    h->buffer_offset = sizeof(SharedHeader);
    h->capacity = 256;

    LocalView prod(producer_map), cons(consumer_map);    // each computes its own pointers
    for (int v = 10; v < 17; ++v) {
        auto w = std::atomic_ref(prod.header->write_pos).load(std::memory_order_relaxed);
        prod.slots[w & (prod.header->capacity - 1)] = v;
        std::atomic_ref(prod.header->write_pos).store(w + 1, std::memory_order_release);
    }

    auto end = std::atomic_ref(cons.header->write_pos).load(std::memory_order_acquire);
    auto r = std::atomic_ref(cons.header->read_pos).load(std::memory_order_relaxed);
    std::printf("consumer read %zu values:", end - r);
    for (; r != end; ++r) std::printf(" %d", cons.slots[r & (cons.header->capacity - 1)]);
    std::atomic_ref(cons.header->read_pos).store(r, std::memory_order_release);
    std::printf("\n");
}
