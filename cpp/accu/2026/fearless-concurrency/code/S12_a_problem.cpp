int main()
{
    std::vector<int> vec{1, 2, 3, 4, 5};

    auto thread = spawn_thread([](auto span) {
        std::print("{}", span);
    }, std::span<int const>(vec));

    vec.resize(100); // Uh oh

    thread.join();
}
