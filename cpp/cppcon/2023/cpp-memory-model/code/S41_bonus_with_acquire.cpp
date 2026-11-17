void IncsharedXtimeAquire(){
    for (int count{0}; count < 10000000;) {
        int expected{};
        if (flag.compare_exchange_strong(expected, 1,
        std::memory_order_acquire)){
            //lock successfull
            shared_val++;
            count++;
            flag.store(0, std::memory_order_release);
        }
    }
}
