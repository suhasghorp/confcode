#include <generator>
#include <iostream>
#include <string>
#include <vector>

// Callback style: state lives in an object, logic lives in a callback.
struct line_builder {
    std::size_t width;
    std::string current;
    void on_piece(const std::string& piece, auto&& emit)
    {
        if (current.size() + piece.size() + 1 > width) {
            emit(current);
            current = piece;
        } else if (current.empty()) {
            current = piece;
        } else {
            current += " " + piece;
        }
    }
};

// Coroutine style: state is just local variables in a loop.
std::generator<std::string> lines(std::vector<std::string> pieces, std::size_t width)
{
    std::string current;
    for (auto& piece : pieces) {
        if (current.size() + piece.size() + 1 > width) {
            co_yield current;
            current = piece;
        } else if (current.empty()) {
            current = piece;
        } else {
            current += " " + piece;
        }
    }
    if (!current.empty()) co_yield current;
}

int main()
{
    std::vector<std::string> words{"asynchronous", "io", "and", "coroutines", "for", "smooth", "streaming"};

    line_builder b{20, {}};
    for (auto& w : words) b.on_piece(w, [](const std::string& l) { std::cout << "cb: " << l << '\n'; });
    if (!b.current.empty()) std::cout << "cb: " << b.current << '\n';   // callbacks need an explicit end-of-stream step

    for (auto&& l : lines(words, 20)) std::cout << "co: " << l << '\n';
}
