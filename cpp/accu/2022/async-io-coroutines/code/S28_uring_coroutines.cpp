template <size_t line_length>
task<std::string> concatenate_to(task<std::string>& in)
{
  std::string current_line;
  for (;;) {
    auto next_piece = co_await in;
    if (current_line.length() + next_piece.length() + 1 > line_length) {
      co_yield std::exchange(current_line, next_piece);
    } else if (current_line.empty()) {
      current_line = next_piece;
    } else {
      current_line += " " + next_piece;
    }
  }
}
