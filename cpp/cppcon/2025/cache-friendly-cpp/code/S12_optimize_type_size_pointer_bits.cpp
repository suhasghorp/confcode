class ContainerOrText
{
    std::uintptr_t _impl;

public:
    bool is_container() const { return (_impl & 0b0) == 0; }
    Container* as_container() const { return (Container*)(_impl); }
    Text* as_text() const { return (Text*)(_impl & ~uintptr_t(0b0)); }
};
