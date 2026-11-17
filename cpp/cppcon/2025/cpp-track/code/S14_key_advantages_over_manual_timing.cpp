void important_function() {
    auto start = high_resolution_clock::now();
    // Simulate some work
    this_thread::sleep_for(milliseconds(100));
    auto end = high_resolution_clock::now();
    auto duration =
duration_cast<milliseconds>(end - start);
    cout << "took: " << duration.count() << "
ms" << endl;
}
