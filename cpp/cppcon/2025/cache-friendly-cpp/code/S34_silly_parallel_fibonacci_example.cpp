void accumulate_fib(unsigned& result, unsigned n)
{
    if (n < 2)
    {
        result += n;
    }
    else
    {
        accumulate_fib(result, n - 1);
        accumulate_fib(result, n - 2);
    }
}
