void important_function() {
    CTRACK; // Just add this one line!
    // Simulate some work
    this_thread::sleep_for(milliseconds(100));
}
// Print CTRACK
ctrack::result_print()
