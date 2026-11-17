#include <array>
#include <iostream>
#include <optional>

// Slots hold a value, are Empty, or are a Tombstone (Deleted).
enum class state { empty, full, deleted };
struct slot { state st = state::empty; int value = 0; };

std::array<slot, 8> table;
std::size_t home(int v) { return static_cast<std::size_t>(v) % table.size(); }

void insert(int v)
{
    for (std::size_t i = home(v);; i = (i + 1) % table.size())
        if (table[i].st != state::full) { table[i] = {state::full, v}; return; }
}

bool find(int v)
{
    for (std::size_t i = home(v);; i = (i + 1) % table.size()) {
        if (table[i].st == state::empty) return false;                 // only EMPTY stops the search
        if (table[i].st == state::full && table[i].value == v) return true;
    }
}

void erase_wrong(int v) { for (auto& s : table) if (s.st == state::full && s.value == v) s.st = state::empty; }
void erase_right(int v) { for (auto& s : table) if (s.st == state::full && s.value == v) s.st = state::deleted; }

int main()
{
    insert(5); insert(13);             // 13 % 8 == 5: collides, probes to slot 6
    erase_right(5);
    std::cout << "with tombstone, find(13): " << find(13) << '\n';   // 1

    table = {};
    insert(5); insert(13);
    erase_wrong(5);
    std::cout << "with plain empty, find(13): " << find(13) << '\n'; // 0: lost!
}
