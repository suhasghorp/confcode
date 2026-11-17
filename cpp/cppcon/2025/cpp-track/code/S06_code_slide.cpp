// Fast function called many times - often missed by sampling
void micro_operation()
{
    CTRACK;
    this_thread::sleep_for(milliseconds(1));
}
// Function with high variance - sampling misses the pattern
void variable_timing_function(bool slow_path)
{
    CTRACK;
    if (slow_path)
    {
        this_thread::sleep_for(milliseconds(100));
    }
    else
    {
        this_thread::sleep_for(milliseconds(1));
    }
}
