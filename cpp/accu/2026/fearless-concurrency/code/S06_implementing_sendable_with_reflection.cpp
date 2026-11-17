// ...continued

if constexpr (is_class_type(info)) {
    // Polymorphic classes must also be final
    if constexpr (is_polymorphic_type(info) && not is_final_type(info)) {
        return false;
    }
    constexpr auto ctx = access_context::unchecked();

    // All bases must be sendable
    template for (constexpr auto base :
                     define_static_array(bases_of(info, ctx))) {
        using base_type = [:type_of(base):];
        if constexpr (not is_sendable_v<typename[:type_of(base):]>) {
            return false;
        }
    }

    // All NSDMs must be sendable and not declared mutable
    template for (constexpr auto member :
                    define_static_array(nonstatic_data_members_of(info, ctx))) {
        if constexpr (is_mutable_member(member)) {
            return false;
        }
        if constexpr (not is_sendable_v<typename[:type_of(member):]>) {
            return false;
        }
    }
    return true;
}
