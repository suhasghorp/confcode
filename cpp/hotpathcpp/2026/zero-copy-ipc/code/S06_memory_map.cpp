// Page Alignment Enforcement & Validation:
absl::Status MemoryMap::DoMap(int fd, size_t size, int prot, bool use_mirror) {
  const size_t page_size = sysconf(_SC_PAGESIZE); // 4096 bytes on x86_64
  if (size % page_size != 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Buffer size %lu must be multiple of page size %lu",
                        size, page_size));
  }
  // Linux kernel requires strict page alignment for MAP_FIXED;
  // non-aligned offsets trigger instantaneous EINVAL error!
  return absl::OkStatus();
}
