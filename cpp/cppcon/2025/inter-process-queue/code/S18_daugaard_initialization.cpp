// Initialize. Buffer must have required alignment. Size must be a
// power of two.
void Initialize(void* buffer, size_t size)
{
    Reset();
    m_Reader.buffer = m_Writer.buffer = static_cast<char*>(buffer);
    m_Reader.size = m_Writer.size = m_Writer.end = size;
}

void Reset()
{
    m_Reader = m_Writer = LocalState();
    m_ReaderShared.pos = m_WriterShared.pos = 0;
}
