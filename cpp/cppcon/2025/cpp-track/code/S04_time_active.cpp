void slow_function() {
    CTRACK;
    this_thread::sleep_for(milliseconds(900));
}
void fast_function() {
    CTRACK;
    this_thread::sleep_for(milliseconds(100));
    slow_function();
}
