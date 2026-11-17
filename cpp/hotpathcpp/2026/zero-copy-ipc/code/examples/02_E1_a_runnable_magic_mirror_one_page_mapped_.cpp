// The "magic mirror": map the same pages twice, side by side.
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

int main() {
  const size_t size = static_cast<size_t>(sysconf(_SC_PAGESIZE));  // one page = 4096
  int fd = memfd_create("ring", 0);                 // anonymous shared memory file
  if (fd < 0 || ftruncate(fd, size) != 0) { std::perror("memfd"); return 1; }

  // 1. Reserve 2 * size of address space; nothing is accessible yet.
  auto* base = static_cast<uint8_t*>(
      mmap(nullptr, 2 * size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
  // 2. and 3. Map the same file offset 0 into both halves.
  void* first = mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  void* second = mmap(base + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  if (base == MAP_FAILED || first == MAP_FAILED || second == MAP_FAILED) { std::perror("mmap"); return 1; }

  // Write a 32-byte message that starts 12 bytes before the end of the buffer.
  const char msg[] = "this message wraps the ring end";  // 31 chars + '\0'
  size_t offset = size - 12;
  std::memcpy(base + offset, msg, sizeof msg);           // one memcpy, no split

  std::printf("read back as one span: \"%s\"\n", reinterpret_cast<char*>(base + offset));
  std::printf("first 20 bytes of the buffer: \"%.20s\"\n", reinterpret_cast<char*>(base));
  std::printf("same physical byte: base[0] == base[size]? %s\n", base[0] == base[size] ? "yes" : "no");

  // The kernel rejects a mirror at an address that is not page aligned.
  void* bad = mmap(base + size + 1, size, PROT_READ, MAP_SHARED | MAP_FIXED, fd, 0);
  std::printf("MAP_FIXED at an unaligned address: %s\n", bad == MAP_FAILED ? std::strerror(errno) : "mapped?!");
}
