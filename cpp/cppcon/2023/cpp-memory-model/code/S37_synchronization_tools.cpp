int main(){
    std::packaged_task sum_task([](int a, int b){
        return a+b; });

    std::packaged_task mul_task([](int a, int b){ return a*b; });

    mul_task(10, 2);
    sum_task(10, mul_task.get_future().get());

    fmt::print("tot: {}", sum_task.get_future().get());

    return 0;
}
