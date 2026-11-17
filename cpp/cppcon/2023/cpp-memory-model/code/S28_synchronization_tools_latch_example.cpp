int main(){
    std::vector<std::string_view> ppl{"Alex", "Dani", "Benny", "Guy"};
    std::vector<std::jthread> workers;
    std::latch jobs{ppl.size()}, go_home{1};
    auto job = [&jobs, &go_home](std::string_view name){
        fmt::print("{} is doing a job\n", name);
        jobs.count_down();
        go_home.wait();
        fmt::print("{} is going home\n", name);
    };

    for (const auto& p : ppl) {
        workers.push_back(std::jthread(job, p));
    }
    jobs.wait();
    fmt::print("Go home!\n");
    go_home.count_down();
}
