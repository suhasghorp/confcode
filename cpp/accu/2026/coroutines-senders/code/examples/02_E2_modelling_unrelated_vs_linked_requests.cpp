#include <iostream>
#include <string>
#include <vector>

struct Request {
    int user_id;
    std::string action;
};

// A deliberately simple dependency rule:
// requests from the same user are linked, others are independent.
bool must_preserve_order(const Request& a, const Request& b) {
    return a.user_id == b.user_id;
}

int main() {
    std::vector<Request> incoming{
        {1, "login"},     // Alice
        {2, "login"},     // Bob
        {1, "purchase"},  // Alice again
    };

    for (std::size_t i = 0; i < incoming.size(); ++i) {
        for (std::size_t j = i + 1; j < incoming.size(); ++j) {
            const auto& a = incoming[i];
            const auto& b = incoming[j];
            std::cout << a.action << "(user " << a.user_id << ") -> "
                      << b.action << "(user " << b.user_id << "): "
                      << (must_preserve_order(a, b) ? "linked" : "independent")
                      << '\n';
        }
    }
}

