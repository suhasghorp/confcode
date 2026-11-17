static constexpr int prot = PROT_READ | PROT_WRITE;
int const flags = [&] {
    int f = MAP_SHARED;
    if (addr) f |= MAP_FIXED;
    if (fd < 0) f |= MAP_ANONYMOUS;
    return f;
}();
auto const mapped = ::mmap(
    addr, len, prot, flags, fd, off_t(offset));
