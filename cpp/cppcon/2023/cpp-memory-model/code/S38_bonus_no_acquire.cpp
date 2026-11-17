std::atomic<int> flag{0};
uint64_t shared_val{};
std::mt19937_64 eng{std::random_device{}()};
std::uniform_int_distribution dist{100, 500};

void IncsharedXtimeNoAquire(){
    for (int count{0}; count < 10000000;) {
        int expected{};
        if (flag.compare_exchange_strong(expected, 1,
        std::memory_order_relaxed)){
            //lock successfull
            shared_val++;
            count++;
            flag.store(0, std::memory_order_relaxed);
        }
    }
}
