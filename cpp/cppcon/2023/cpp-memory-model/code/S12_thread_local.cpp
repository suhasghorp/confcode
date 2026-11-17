thread_local uint64_t inc = 0;

void do_count(std::string_view name, int count,
              uint64_t& ret, uint64_t* val){
    for (int i=0; i<count; i++){
        (*val)++;
    }

    ret = *val;
}

int main(){
    uint64_t ra{},rb{};
    std::thread a(do_count, "A", 10, std::ref(ra), &inc);
    std::thread b(do_count, "A", 20, std::ref(rb), &inc);
    a.join(); b.join();
    fmt::print("ret a: {}\nret b: {}\nmain inc: {}\n", ra, rb, inc);
}
