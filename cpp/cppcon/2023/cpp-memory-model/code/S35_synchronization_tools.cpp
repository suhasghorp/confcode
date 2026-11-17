int main(){
    std::once_flag flag;
    auto f = [&flag](){
        static int x{10};
        std::call_once(flag, [](){x++;});
        fmt::print("{}\n", x);
    };

    std::jthread t1(f);
    std::jthread t2(f);
}
