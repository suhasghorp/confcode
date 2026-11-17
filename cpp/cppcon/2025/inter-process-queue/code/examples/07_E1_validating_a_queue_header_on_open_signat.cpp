#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <expected>
#include <type_traits>
#include <utility>

enum struct MsgSize : std::uint32_t {};               // strong types: same bits, different types
enum struct Capacity : std::uint64_t {};

struct QueueHeader {                                   // lives at offset 0 of the shared region
    std::array<char, 32> signature;
    std::array<std::byte, 16> uuid;
    Capacity capacity;
    MsgSize max_msg_size;
};
static_assert(std::is_trivially_copyable_v<QueueHeader> && std::is_standard_layout_v<QueueHeader>);

enum class OpenError { wrong_kind, wrong_queue, too_small };

std::expected<QueueHeader const*, OpenError>
open_queue(void const* region, char const* kind, std::array<std::byte, 16> const& uuid, MsgSize need) {
    auto const* h = static_cast<QueueHeader const*>(region);
    if (std::strncmp(h->signature.data(), kind, h->signature.size()) != 0)
        return std::unexpected(OpenError::wrong_kind);
    if (h->uuid != uuid) return std::unexpected(OpenError::wrong_queue);    // stale file at the same path?
    if (std::to_underlying(h->max_msg_size) < std::to_underlying(need))
        return std::unexpected(OpenError::too_small);
    return h;
}

int main() {
    alignas(QueueHeader) std::byte region[sizeof(QueueHeader)]{};
    auto* h = new (region) QueueHeader{};                 // the creator initialises the header
    std::strncpy(h->signature.data(), "spsc-bytes-v1", h->signature.size());
    h->uuid = {std::byte{0x12}, std::byte{0x34}};
    h->capacity = Capacity{1 << 20};
    h->max_msg_size = MsgSize{256};

    std::array<std::byte, 16> other_uuid{std::byte{0x99}};
    auto ok  = open_queue(region, "spsc-bytes-v1", h->uuid, MsgSize{128});
    auto bad = open_queue(region, "spsc-bytes-v1", other_uuid, MsgSize{128});
    std::printf("same uuid: %s, other uuid: %s\n", ok ? "ok" : "rejected",
                bad ? "ok" : (bad.error() == OpenError::wrong_queue ? "rejected (wrong queue)" : "rejected"));
}
