int main(){
    std::atomic<bool> flag(true);
    auto holder = std::thread([&flag](){
        std::this_thread::sleep_for(1s);
        fmt::print("Holder releasing lock \n");
        flag = false;});

    auto worker = std::thread(spin_bool_foo, std::ref(flag));

    holder.join();
    worker.join();
}
