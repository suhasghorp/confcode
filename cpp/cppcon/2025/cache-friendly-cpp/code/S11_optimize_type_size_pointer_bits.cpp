class ContainerOrText
{
    bool _is_container;
    void* _ptr;

public:
    bool is_container() const { return _is_container; }
    Container* as_container() const { return (Container*)_ptr; }
    Text* as_text() const { return (Text*)_ptr; }
};
