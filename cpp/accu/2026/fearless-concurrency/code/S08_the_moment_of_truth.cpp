int main()
{
    std::string str = "Hello";

    auto thread = spawn_thread([](auto sv) {
        std::println("{}", sv);
    }, std::string_view(str));

    thread.join();
}
