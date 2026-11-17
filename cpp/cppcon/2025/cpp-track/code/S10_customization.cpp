void complex_algorithm() {
    CTRACK; // Track the whole function
    {
        CTRACK_NAME("algorithm_init");
        this_thread::sleep_for(milliseconds(10));
    }
    for (int i = 0; i < 3; ++i) {
        CTRACK_NAME("algorithm_iteration");
        this_thread::sleep_for(milliseconds(15));
    }
}
