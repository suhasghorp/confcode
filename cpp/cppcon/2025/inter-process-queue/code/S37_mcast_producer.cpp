inline void
Producer::Info::
finish_write()
{
    shared->read_counter.store(
        local_counter, std::memory_order_release);

    // Update the saved_counter because the write operation has been
    // committed, and there is no longer a need to restore the original
    // value.
    saved_counter = local_counter;
}
