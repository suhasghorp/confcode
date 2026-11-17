// The Cost of Kernel Indirection (Unix Domain Sockets / Pipes)
void BenchmarkUnixSocketTransfer(benchmark::State& state) {
  for (auto _ : state) {
    // Syscall 1: Trap from User Space (Ring 3) into Kernel (Ring 0)
    ssize_t sent = send(socket_fd, payload.data(), payload.size(), 0);
    // Kernel copies data into sk_buff queue; CPU flushes TLB on context switch

    // Syscall 2: Kernel -> User space transition
    ssize_t recvd = recv(client_fd, buffer.data(), buffer.size(), 0);
    // Result: ~1,240 ns latency tax, store buffer stalls, cache thrashing!
  }
}
