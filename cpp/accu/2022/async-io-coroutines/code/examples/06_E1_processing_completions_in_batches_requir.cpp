#include <liburing.h>

#include <array>

// Process every completion currently in the ring with one wait at most.
template <typename Handler>
unsigned drain_completions(io_uring& ring, Handler&& handle)
{
    io_uring_submit_and_wait(&ring, 1);                 // submit pending SQEs, wait for >= 1 CQE

    std::array<io_uring_cqe*, 32> cqes;
    unsigned n = io_uring_peek_batch_cqe(&ring, cqes.data(), cqes.size());
    for (unsigned i = 0; i < n; ++i) {
        handle(cqes[i]);                                // may queue new SQEs
    }
    io_uring_cq_advance(&ring, n);                     // mark all n as seen at once
    return n;
}

int main()
{
    io_uring ring;
    io_uring_queue_init(8, &ring, 0);
    // ... queue reads with io_uring_prep_read ...
    io_uring_queue_exit(&ring);
}
