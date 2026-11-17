int main()
{
    const int micro_op_count = 1000;
    for (int i = 0; i < micro_op_count; ++i)
    {
        micro_operation();
    }
    for (int i = 0; i < 100; ++i)
    {
        bool slow = i % 10 == 0; // Every 10th call is slow
        variable_timing_function(slow);
    }
    return 0;
}
