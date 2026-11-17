// Rate-Limited Syscall Optimization via Hardware TSC:
uint64_t current_tsc = __rdtsc();
if (current_tsc - last_tsc > 100000ULL) {
  now = absl::ToUnixNanos(absl::Now()); // Sample wall-clock
  last_tsc = current_tsc;
}

// SAFE RECLAMATION: Evict dead/hung nodes to prevent global deadlock
if (now - header_->readers[i].heartbeat_ts.load(std::memory_order_relaxed) >
    evict_timeout_ns_) {
  header_->readers[i].active.store(false, std::memory_order_release);
  continue; // Unblocks producer immediately!
}
