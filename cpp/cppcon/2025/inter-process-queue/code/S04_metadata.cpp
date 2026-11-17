template <typename PidLockT>
requires ProcessIdLockC<PidLockT>
struct MetaData
{
    using PidLock = PidLockT;

    enum struct BufferOffset : std::size_t
    {
    };

    constexpr MetaData() = default;
    explicit MetaData(Characteristics const & c);
    explicit MetaData(
        Characteristics const & c,
        BufferOffset buffer_offset);
