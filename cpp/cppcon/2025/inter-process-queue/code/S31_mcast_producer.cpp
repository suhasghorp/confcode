    auto call = [&]() -> R {
        return std::invoke(
            std::forward<InvocableT>(invocable),
            writer,
            std::forward<ArgTs>(args)...);
    };
    if constexpr (std::is_same_v<R, void>) {
        call();
        info_.finish_write();
        return;
    } else {
        R result = call();
        info_.finish_write();
        return result;
    }
}
