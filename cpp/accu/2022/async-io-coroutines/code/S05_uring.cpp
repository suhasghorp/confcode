#include <liburing.h>

class ring
{
public:
  using work = std::function<bool(std::span<char>)>;
  ring();
  ring& operator=(ring&&) = delete;
  ~ring();

  void add(int fd, work);
  void wait();
private:
  struct read_work;
  std::list<read_work> pending_;
  io_uring uring_;
};
