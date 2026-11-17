#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

constexpr std::int64_t no_data = 0;
constexpr std::int64_t lapped = -1;

struct MockConsumer {                       // returns a scripted sequence of outcomes
    std::vector<std::string_view> script{"hello", "", "a much longer message that needs more room", "LAPPED"};
    std::size_t next = 0;
    std::expected<std::size_t, std::int64_t> try_read(std::span<std::byte> data) {
        if (next == script.size()) return std::unexpected(no_data);
        auto m = script[next];
        if (m.empty())      { ++next; return std::unexpected(no_data); }
        if (m == "LAPPED")  { ++next; return std::unexpected(lapped); }
        if (m.size() > data.size()) return std::unexpected(std::int64_t(m.size()));   // need more room
        std::memcpy(data.data(), m.data(), m.size());
        ++next;
        return m.size();
    }
};

int main() {
    MockConsumer consumer;
    std::vector<std::byte> buf(16);
    for (int i = 0; i < 5; ++i) {
        auto r = consumer.try_read(buf);
        if (r) {
            std::printf("got %zu bytes: %.*s\n", *r, int(*r), reinterpret_cast<char const*>(buf.data()));
        } else if (r.error() == no_data) {
            std::puts("nothing yet");
        } else if (r.error() == lapped) {
            std::puts("lapped: messages were lost; resynchronising");
        } else {
            std::printf("buffer too small, need %lld bytes; growing\n", static_cast<long long>(r.error()));
            buf.resize(std::size_t(r.error()));
        }
    }
}
