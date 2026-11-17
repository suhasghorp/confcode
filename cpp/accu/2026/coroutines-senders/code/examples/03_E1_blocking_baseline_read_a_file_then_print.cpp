#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Blocking: does not return until the whole file has been read.
std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    auto data = read_file("input.txt"); // thread waits here for the disk
    std::cout << data;                 // safe: data is guaranteed ready
}

