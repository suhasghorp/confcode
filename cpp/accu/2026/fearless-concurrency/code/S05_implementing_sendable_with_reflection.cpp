// ...continued

// Member pointer types are *not* sendable
if constexpr (is_member_pointer_type(info)) {
    return false;
}

// Pointer types are sendable if the pointee is a function type,
// or if the pointee is both const and sendable
if constexpr (is_pointer_type(info)) {
    constexpr auto pointee = remove_pointer(info);
    if constexpr (is_function_type(pointee)) {
        return true;
    } else {
        return is_const_type(pointee)
            && is_sendable_v<typename[:remove_const(pointee):]>;
    }
}

if constexpr (is_reference_type(info)) {
    constexpr auto referent = remove_reference(info);
    if constexpr (is_function_type(referent)) {
        return true;
    } else {
        return is_const_type(referent)
            && is_sendable_v<typename[:remove_const(referent):]>;
    }
}
