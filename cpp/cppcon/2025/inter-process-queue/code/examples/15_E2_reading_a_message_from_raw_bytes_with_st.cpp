#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

struct Trade {                       // an implicit-lifetime message type
    std::uint64_t id;
    double price;
    std::int32_t quantity;
};

int main() {
    alignas(Trade) std::byte region[sizeof(Trade)];     // stands in for mapped shared memory
    Trade t{42, 101.25, 300};
    std::memcpy(region, &t, sizeof t);                    // the "producer" writes bytes

    // The "consumer" declares that these bytes are a Trade, keeping their values:
    Trade const* msg = std::start_lifetime_as<Trade>(region);
    std::printf("trade %llu: %d @ %.2f\n",
                static_cast<unsigned long long>(msg->id), msg->quantity, msg->price);
}
