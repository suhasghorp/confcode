std::binary_semaphore start_speaking{0}, p1_done{0}, p2_done{0};

void person_speak(std::stop_token s, std::string_view text, std::binary_semaphore& w, std::binary_semaphore& r){
    w.acquire();
    while (not s.stop_requested()){
        fmt::print("{}", text);
        std::this_thread::sleep_for(1s);
    }
    r.release();
}

void stop_speaking(std::array<std::stop_source, 2> ssrc){
    for (auto i = 0u; i < ssrc.size(); i++){
        std::this_thread::sleep_for(2s);
        ssrc[i].request_stop();
    }
}


int main() {
    std::jthread p1(person_speak, "Hello Peson2 im Person1\n",
                    std::ref(start_speaking), std::ref(p1_done));
    std::jthread p2(person_speak, "Hello Peson1 im Person2\n",
                    std::ref(p1_done), std::ref(p2_done));
    start_speaking.release();
    std::jthread sched(stop_speaking, std::array{p1.get_stop_source(), p2.get_stop_source()});
    p2_done.acquire();
    fmt::print("Goodby!");
}
