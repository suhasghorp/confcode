#include <iostream>
#include <string>

// One thread per connection: the 'state' is simply where we are in the code.
bool handle_connection(std::istream& in, std::ostream& out) {
    std::string line;

    if (!std::getline(in, line) || line != "HELLO") {      // blocks
        out << "error: expected HELLO\n";
        return false;
    }
    out << "HELLO-ACK\n";

    if (!std::getline(in, line) || line != "AUTH secret") { // blocks
        out << "error: bad credentials\n";
        return false;
    }
    out << "AUTH-OK\n";

    while (std::getline(in, line) && line != "CLOSE")      // blocks
        out << "ECHO " << line << '\n';

    return true;
}

int main() {
    handle_connection(std::cin, std::cout);
}

