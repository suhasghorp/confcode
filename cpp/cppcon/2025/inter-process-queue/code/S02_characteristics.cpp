struct Characteristics
{
    struct Signature
    {
        std::array<char, 32> value_;

        std::string str() const
        {
            auto v = std::string_view(value_.data(), value_.size());
            return std::string(
                v.data(), std::min(v.size(), v.find('\0')));
        }

        constexpr auto operator <=> (Signature const &) const = default;
    };
