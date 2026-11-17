#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T> concept TriviallyCopyableC = std::is_trivially_copyable_v<T>;

class Producer {
    alignas(8) std::byte buffer_[1024];
    std::size_t committed_ = 0;        // what readers may see (an atomic in a real queue)
    std::size_t local_ = 0;            // where the current write has got to
public:
    class Writer {
        Producer& p_;
        std::size_t saved_;
    public:
        explicit Writer(Producer& p) : p_(p), saved_(p.local_) {}
        ~Writer() { p_.local_ = saved_; }                         // roll back unless committed
        void commit() { saved_ = p_.local_; }
        template <TriviallyCopyableC T>
        T& emplace(auto&&... args) requires std::constructible_from<T, decltype(args)...> {
            p_.local_ = (p_.local_ + alignof(T) - 1) / alignof(T) * alignof(T);   // align
            auto* t = new (static_cast<void*>(p_.buffer_ + p_.local_)) T(std::forward<decltype(args)>(args)...);
            p_.local_ += sizeof(T);
            return *t;
        }
        void write(std::span<std::byte const> data) {
            std::memcpy(p_.buffer_ + p_.local_, data.data(), data.size());
            p_.local_ += data.size();
        }
    };

    template <class F, class... Args>
    std::invoke_result_t<F, Writer&, Args...> write(F&& f, Args&&... args)
        noexcept(std::is_nothrow_invocable_v<F, Writer&, Args...>)
    {
        Writer w(*this);
        if constexpr (std::is_void_v<std::invoke_result_t<F, Writer&, Args...>>) {
            std::invoke(std::forward<F>(f), w, std::forward<Args>(args)...);
            w.commit(); committed_ = local_;
        } else {
            auto r = std::invoke(std::forward<F>(f), w, std::forward<Args>(args)...);
            w.commit(); committed_ = local_;
            return r;
        }
    }
    std::size_t committed() const { return committed_; }
};

struct Header { int type; int length; };

int main() {
    Producer p;
    char const text[] = "hello, world";
    int r = p.write([&](Producer::Writer& w, int type) {
        w.emplace<Header>(type, int(sizeof text));
        w.write(std::as_bytes(std::span(text)));
        return 42;
    }, 7);
    std::printf("committed %zu bytes, returned %d\n", p.committed(), r);

    try {
        p.write([](Producer::Writer& w) {
            w.emplace<Header>(1, 0);
            throw std::runtime_error("oops");          // half-written message is discarded
        });
    } catch (std::exception const& e) {
        std::printf("callback threw: %s; still %zu bytes committed\n", e.what(), p.committed());
    }
}
