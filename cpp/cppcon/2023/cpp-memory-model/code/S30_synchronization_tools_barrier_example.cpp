int main(){
    std::vector<std::string_view> ppl{"Alex", "Dani", "Benny", "Guy"};
    std::vector<std::jthread> workers;
    auto on_complete = [](){
        static int x = 0;
        not x++ ? fmt::print("Go Home!\n") : fmt::print("Done!\n"); };
    std::barrier sync{ppl.size(), on_complete};
    auto job = [&sync](std::string_view name){
        fmt::print("{} is doing a job\n", name);
        sync.arrive_and_wait();
        fmt::print("{} is going home\n", name);
        sync.arrive_and_wait();
    };

    for (const auto& p : ppl) {
        workers.push_back(std::jthread(job, p));
    }
}
