struct Event
{
    time_point start_time;
    time_point end_time;
    std::string_view filename;
    std::string_view function;
    int line;
    int thread_id;
    unsigned int event_id;
}
