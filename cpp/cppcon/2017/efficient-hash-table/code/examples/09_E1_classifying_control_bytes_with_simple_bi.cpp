#include <cstdint>
#include <iostream>

using ctrl_t = signed char;
enum Ctrl : ctrl_t { kEmpty = -128, kDeleted = -2, kSentinel = -1 };

bool is_full(ctrl_t c)             { return c >= 0; }            // high bit clear
bool is_empty_or_deleted(ctrl_t c) { return c < kSentinel; }     // -128 or -2

std::size_t H1(std::size_t hash) { return hash >> 7; }
ctrl_t      H2(std::size_t hash) { return hash & 0x7F; }

int main()
{
    std::size_t hash = 0x7FDF9632F1F8EB43;          // the talk's example hash: 7F DF 96 32 F1 F8 EB 43
    std::cout << std::hex << "H1 = " << H1(hash) << ", H2 = " << int(H2(hash)) << '\n';   // H2 = 43
    for (ctrl_t c : {ctrl_t(kEmpty), ctrl_t(kDeleted), ctrl_t(kSentinel), H2(hash)})
        std::cout << std::dec << int(c) << ": full=" << is_full(c)
                  << " empty_or_deleted=" << is_empty_or_deleted(c) << '\n';
}
