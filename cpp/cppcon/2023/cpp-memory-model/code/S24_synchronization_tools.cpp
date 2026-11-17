std::binary_semaphore start_speaking{0}, person1_spoke{0},
                      person2_spoke{0};


void person_speak(std::string_view text, std::binary_semaphore& w, std::binary_semaphore& r){
    w.acquire();
    fmt::print("{}", te/* ? */
    std::this_thread::s/* ? */
    r.release();
}


int main(){
    std::jthread p1(per/* ? */ref(start_speaking), std::ref(person1_spoke));
    std::jthread p2(per/* ? */ref(person1_spoke), std::ref(person2_spoke));
    start_speaking.release();
    person2_spoke.acquire();

    fmt::print("GoodBye!");

    return 0;
}
