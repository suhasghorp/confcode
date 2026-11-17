template <typename T, str name>
struct promise
{
  task<T,name> get_return_object() noexcept {
    std::cerr << "task<T, " << name << ">::get_return_object()\n";
    return {this};
  }
…
};

template <str name>
task<void, "print_lines"> print_lines(std::ostream& os, task<std::string, name>& in)
{
…
