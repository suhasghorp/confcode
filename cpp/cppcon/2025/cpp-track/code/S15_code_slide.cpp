// Global store with per-thread vectors
struct store {
    inline static std::deque<t_events> a_events{};   // One vector per thread
    inline static std::mutex event_mutex;            // Used ONLY during
setup
};
// Thread-local pointers to this thread's buffer
inline thread_local t_events* event_ptr = nullptr;
// First-time thread setup (happens once on thread spawn)
int fetch_event_t_id() {
    if (thread_id == nullptr) {
        std::scoped_lock lock(store::event_mutex); // ONLY lock
        store::a_events.emplace_back(t_events{});   // Create buffer
        event_ptr = &store::a_events[*thread_id];   // Point to it
        event_ptr->reserve(100);                    // Pre-allocate
    }
    return *thread_id;
}
// Recording (no locks!)
event_ptr->emplace_back(Event{...}); // Just append to vector
