#include <chrono>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

struct Event { unsigned id, parent; long long ns; const char* name; };

struct store {
    inline static std::deque<std::vector<Event>> buffers;   // deque: existing buffers never move
    inline static std::mutex mutex;                          // used only when a thread registers
};
inline thread_local std::vector<Event>* buf = nullptr;
inline thread_local unsigned current_id = 0, id_counter = 0;

std::vector<Event>& my_buffer()
{
    if (!buf) {
        std::scoped_lock lock(store::mutex);                 // once per thread
        buf = &store::buffers.emplace_back();
        buf->reserve(1024);
    }
    return *buf;
}

class Track {
    unsigned id_, parent_;
    const char* name_;
    std::chrono::steady_clock::time_point t0_ = std::chrono::steady_clock::now();
public:
    explicit Track(const char* name) : id_(++id_counter), parent_(current_id), name_(name) { current_id = id_; }
    ~Track()
    {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0_).count();
        my_buffer().push_back({id_, parent_, ns, name_});    // lock-free: only this thread uses it
        current_id = parent_;                                // restore parent
    }
};

void leaf()  { Track t("leaf");  std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
void outer() { Track t("outer"); leaf(); leaf(); }

int main()
{
    { std::jthread a(outer), b(outer); }
    for (auto& v : store::buffers)
        for (auto& e : v) std::cout << e.name << " id " << e.id << " parent " << e.parent << '\n';
}
