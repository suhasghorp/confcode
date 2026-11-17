#include <liburing.h>

io_uring uring;
io_uring_queue_init(8, &uring, 0);

...

auto entry = io_uring_get_sqe(&uring);
io_uring_prep_read(entry, fd, ptr, size, 0);
io_uring_sqe_set_data(entry, work);

...

io_uring_cqe* entry;
auto e = io_uring_wait_cqe(&uring, &entry);
