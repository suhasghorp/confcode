int main(){
    std::mutex m;
    auto doer = [&m](int idx){
        std::lock_guard l(m);
        fmt::print("doer{} thread doing stuff\n", idx);
    };
    std::jthread t1(doer, 0);
    std::jthread t2(doer, 1);
}
