// Map the file over the first half of the anonymous mapping.
args.fd = saved_fd;
args.len /= 2;
args.offset = offset;
args.addr = mapped->address;
auto tmp = do_mapping(args);
tmp->release();
