struct AdvisoryFileLock
{
    struct Create { mode_t mode; };

    explicit AdvisoryFileLock(std::filesystem::path path);
    explicit AdvisoryFileLock(
        std::filesystem::path path, Create create);

    void swap(AdvisoryFileLock & that) noexcept;

    bool try_lock();
    void lock();
    void unlock() noexcept;

    std::filesystem::path const & path() const;
};
