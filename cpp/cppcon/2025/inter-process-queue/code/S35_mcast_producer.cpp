inline Producer::Writer::
Writer(Info & x)
: info_(x)
{
    // Save the counter, so we can restore it later, if the write
    // operation was aborted for some reason including a user exception.
    info_.saved_counter = info_.local_counter;
}
