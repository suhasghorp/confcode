inline Producer::Writer::
~Writer()
{
    // Restore the counter.  Safe to do always because the tmp_counter is
    // set to the local_counter when finish_write is called.
    info_.local_counter = info_.saved_counter;
}
