int main(){
    std::promise<int> p;
    std::thread t1([&p](){
        std::this_thread::sleep_for(1s);
        p.set_value(42);
    });

    std::thread t2([](auto f){
        fmt::print("The value is {}", f.get());}, p.get_future());

    t1.join();
    t2.join();
}
