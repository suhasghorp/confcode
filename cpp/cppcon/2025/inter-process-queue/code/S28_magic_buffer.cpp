// Map the file over the second half of the anonymous mapping.
args.addr = static_cast<std::byte *>(mapped->address) + args.len;
tmp = do_mapping(args);
tmp->release();

addr_ = std::bit_cast<pointer>(mapped->address);
size_ = mapped->length / 2;
mapped_ = std::move(mapped);
