// A parent process writes into POSIX shared memory; a child process reads it.
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>

struct Mailbox {
  std::atomic<bool> ready{false};
  char text[56];
};

int main() {
  const char* name = "/confcode_hello";
  int fd = shm_open(name, O_CREAT | O_RDWR, 0600);   // creates /dev/shm/confcode_hello
  if (fd < 0 || ftruncate(fd, sizeof(Mailbox)) != 0) { std::perror("shm"); return 1; }
  void* p = mmap(nullptr, sizeof(Mailbox), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED) { std::perror("mmap"); return 1; }
  auto* box = new (p) Mailbox;  // construct the object in the shared pages

  if (fork() == 0) {            // child: same mapping, different process
    while (!box->ready.load(std::memory_order_acquire)) {}
    std::printf("child %d read: %s\n", getpid(), box->text);
    std::fflush(stdout);        // _exit() skips stdio flushing
    _exit(0);
  }
  std::strcpy(box->text, "hello through shared memory");
  box->ready.store(true, std::memory_order_release);   // publish
  wait(nullptr);
  munmap(p, sizeof(Mailbox));
  shm_unlink(name);
}
