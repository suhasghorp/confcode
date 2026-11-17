template <typename InvocableT, typename... ArgTs>
std::invoke_result_t<InvocableT, Producer::Writer &, ArgTs...>
Producer::
write(InvocableT && invocable, ArgTs &&... args)
noexcept(noexcept(std::invoke(
    std::declval<InvocableT>(),
    std::declval<Writer &>(),
    std::declval<ArgTs>()...)))
{
    using R = std::invoke_result_t<InvocableT, Writer &, ArgTs...>;
    auto writer = Writer(info_);
