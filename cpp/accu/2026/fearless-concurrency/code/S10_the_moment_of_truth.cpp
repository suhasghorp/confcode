int main()
{
    std::string str = "Hello";

    auto thread = spawn_thread([&str] {
        str = "goodbye";
    });

    thread.join();
}
