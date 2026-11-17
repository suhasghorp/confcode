class RingBuffer
{
    // Writer and reader's shared positions.
    struct alignas(CACHE_LINE_SIZE) SharedState
    {
        std::atomic<size_t> pos;
    };

    SharedState m_WriterShared;
    SharedState m_ReaderShared;
