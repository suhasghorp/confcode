// Problem: 4 threads × 10ms ≠ 40ms wall time!
sorted_create_grouped_simple_events(events) {
    for (size_t i = 1; i < events.size(); i++) {
        if (result[current].end_time >= events[i].start_time) {
        // Merge overlapping intervals
        result[current].end_time=
        max(result[current].end_time,events[i].end_time);
        } else {
            result.push_back(events[i]); // New interval
        }
    }
}
