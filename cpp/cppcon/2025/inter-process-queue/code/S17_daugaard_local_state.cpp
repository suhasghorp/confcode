class RingBuffer { // ...
    // Writer and reader's local state.
    struct alignas(CACHE_LINE_SIZE) LocalState
    {
        LocalState()
            : buffer(nullptr), pos(0), end(0), base(0), size(0) {}

        char* buffer;
        size_t pos;
        size_t end;
        size_t base;
        size_t size;
    };

    LocalState m_Writer;
    LocalState m_Reader;
