#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct Container { std::vector<int> children; };
struct Text { std::string content; };

static_assert(alignof(Container) >= 2 && alignof(Text) >= 2, "need a free low bit");

class ContainerOrText
{
    static constexpr std::uintptr_t text_tag = 0b1;
    std::uintptr_t _impl;

    explicit ContainerOrText(std::uintptr_t impl) : _impl(impl) {}

public:
    static ContainerOrText container(Container* c)
    {
        return ContainerOrText(reinterpret_cast<std::uintptr_t>(c));
    }
    static ContainerOrText text(Text* t)
    {
        return ContainerOrText(reinterpret_cast<std::uintptr_t>(t) | text_tag);
    }

    bool is_container() const { return (_impl & text_tag) == 0; }
    Container* as_container() const { return reinterpret_cast<Container*>(_impl); }
    Text* as_text() const { return reinterpret_cast<Text*>(_impl & ~text_tag); }
};

static_assert(sizeof(ContainerOrText) == sizeof(void*));

int main()
{
    Container box{{1, 2, 3}};
    Text greeting{"hello"};
    ContainerOrText nodes[] = {ContainerOrText::container(&box), ContainerOrText::text(&greeting)};

    std::printf("sizeof = %zu\n", sizeof(ContainerOrText));
    for (const auto& node : nodes) {
        if (node.is_container())
            std::printf("container: %zu children\n", node.as_container()->children.size());
        else
            std::printf("text: \"%s\"\n", node.as_text()->content.c_str());
    }
}
