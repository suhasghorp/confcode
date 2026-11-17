void spin_bool_foo(std::atomic<bool>& flag){
    while(flag.exchange(true)){
        //spin;
    }
    fmt::print("Worker Spin lock opened doing stuff \n");
    //doing stuff
    fmt::print("Worker releasing lock \n");
    flag = false;
}
