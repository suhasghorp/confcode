int main()
{
    std::vector<int> vec{1, 2, 3, 4, 5};

    fork(
        [&vec] {
            vec.resize(100);
        },
        [span = std::span<int const>(vec)] {
            std::print("{}", span);
        }
    );
}
