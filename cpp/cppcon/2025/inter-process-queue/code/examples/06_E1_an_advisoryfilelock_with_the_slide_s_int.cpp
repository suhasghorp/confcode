#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <utility>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

struct AdvisoryFileLock {
    struct Create { mode_t mode; };

    explicit AdvisoryFileLock(std::filesystem::path path)
        : path_(std::move(path)), fd_(::open(path_.c_str(), O_RDWR | O_CLOEXEC)) { check(); }
    explicit AdvisoryFileLock(std::filesystem::path path, Create create)
        : path_(std::move(path)),
          fd_(::open(path_.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, create.mode)) { check(); }
    AdvisoryFileLock(AdvisoryFileLock&& that) noexcept : fd_(-1) { swap(that); }
    AdvisoryFileLock& operator=(AdvisoryFileLock that) noexcept { swap(that); return *this; }
    ~AdvisoryFileLock() { if (fd_ >= 0) ::close(fd_); }   // closing releases the lock

    void swap(AdvisoryFileLock& that) noexcept { std::swap(path_, that.path_); std::swap(fd_, that.fd_); }

    bool try_lock() {
        if (::flock(fd_, LOCK_EX | LOCK_NB) == 0) return true;
        if (errno == EWOULDBLOCK) return false;
        throw std::system_error(errno, std::generic_category(), "flock");
    }
    void lock() {
        while (::flock(fd_, LOCK_EX) != 0)
            if (errno != EINTR) throw std::system_error(errno, std::generic_category(), "flock");
    }
    void unlock() noexcept { ::flock(fd_, LOCK_UN); }

    std::filesystem::path const& path() const { return path_; }

private:
    void check() const { if (fd_ < 0) throw std::system_error(errno, std::generic_category(), path_.string()); }
    std::filesystem::path path_;
    int fd_;
};

int main() {
    auto file = std::filesystem::temp_directory_path() / "demo_queue.lock";
    AdvisoryFileLock creator(file, AdvisoryFileLock::Create{0600});
    {
        std::unique_lock first(creator, std::try_to_lock);           // works with std::unique_lock
        AdvisoryFileLock other(file);                                  // a second open file description
        std::printf("first: %d, second while first holds it: %d\n",
                    first.owns_lock(), other.try_lock());
        first.unlock();
        std::printf("second after first unlocked: %d\n", other.try_lock());
    }
    std::filesystem::remove(file);
}
