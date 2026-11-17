// Perform an anonymous mapping of twice the requested length.
auto const saved_fd = std::exchange(args.fd, -1);
auto const offset = std::exchange(args.offset, 0);
args.len *= 2;
args.magic = false;
auto mapped = do_mapping(args);
