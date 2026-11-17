int main(){
    int y = 1;
    volatile int* x = &y;

    auto w =std::thread([x](){ for (int i = 0; i < 50; i++){
        *x += 1;}});

    auto r =std::thread([x](){for (int i = 0; i < 50; i++){
        fmt::print("{}\n", *x);}});

    r.join();
    w.join();
}
