atomic<bool> ready_flag{false};
void child_function() {
    CTRACK;
    this_thread::sleep_for(milliseconds(900));
}
//Thread 2
void worker_thread() {
    CTRACK;
    this_thread::sleep_for(milliseconds(100));
    child_function();
    ready_flag.store(true);
}
//Thread 1
void waiting_thread() {
    CTRACK;
    cout << "[Thread 1] Waiting for ready signal..." << endl;
    while (!ready_flag.load()) {
        this_thread::sleep_for(milliseconds(10));
    }
    cout << "[Thread 1] Got signal! Exiting." << endl;
}
