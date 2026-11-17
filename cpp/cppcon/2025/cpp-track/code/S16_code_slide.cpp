// Constructor - Entering a scope
EventHandler(...) {
    previous_event_id = *current_event_id;             // Save parent
    event_id = ++(*current_event_cnt);                 // Get new ID
    *current_event_id = event_id;                      // Become current
}
// Destructor - Leaving a scope
~EventHandler() {
    event_ptr->emplace_back(Event{...});               // Record self
    *current_event_id = previous_event_id;             // Restore parent
    if (previous_event_id > 0) {
        // Register as child of parent
        (*sub_events_ptr)[previous_event_id].push_back(event_id);
    }
}
