int main(){
    std::promise<int> prom1, prom2;
    auto future = prom1.get_future();
    std::vector<int> numbers(500);
    std::iota(numbers.begin(), numbers.end(), 0);
    auto sumTrd = std::thread([&prom1, &numbers](){
        auto sum = std::accumulate(numbers.begin(), numbers.end(), 0);
        prom1.set_value(sum);});
    auto doTrd = std::thread([&future, &prom2](){
        auto sum = future.get();
        prom2.set_value(sum > 100 ? sum/2 : sum + 200);});
    fmt::print("answer is {} \n", prom2.get_future().get());
    sumTrd.join();
    doTrd.join();
}
