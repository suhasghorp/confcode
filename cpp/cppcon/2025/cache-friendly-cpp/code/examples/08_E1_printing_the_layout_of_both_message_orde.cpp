#include <cstddef>
#include <cstdint>
#include <cstdio>

struct Message
{
    std::uint8_t type;
    std::uint32_t length;
    unsigned char* data;
    std::uint16_t checksum;
};

struct MessageReordered
{
    unsigned char* data;
    std::uint32_t length;
    std::uint16_t checksum;
    std::uint8_t type;
};

static_assert(sizeof(void*) != 8 || sizeof(Message) == 24);
static_assert(sizeof(void*) != 8 || sizeof(MessageReordered) == 16);

template <typename M>
void print(const char* name)
{
    std::printf("%-15s type@%zu length@%zu data@%zu checksum@%zu  sizeof=%zu alignof=%zu\n",
                name, offsetof(M, type), offsetof(M, length), offsetof(M, data),
                offsetof(M, checksum), sizeof(M), alignof(M));
}

int main()
{
    print<Message>("declared order");
    print<MessageReordered>("reordered");
}
