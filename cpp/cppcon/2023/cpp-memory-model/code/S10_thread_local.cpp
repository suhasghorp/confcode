thread_local uint64_t inc = 0;

void do_count(std::string_view name, int count, uint64_t& ret){
    for (int i=0; i<count; i++){
        inc++;
    }

    ret = inc;
}

int main(){
    uint64_t ra{},rb{};
    std::thread a(do_count, "A", 10, std::ref(ra));
    std::thread b(do_count, "A", 20, std::ref(rb));
    a.join(); b.join();
    fmt::print("ret a: {}\nret b: {}\nmain inc: {}\n", ra, rb, inc);
}
