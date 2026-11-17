constexpr auto execution_policy =
std::execution::par_unseq;
return std::transform_reduce(
    execution_policy, // Parallel execution
    vec.begin(), vec.end(),
    FieldType{}, std::plus<>(),
    [field](const auto &item) { return item.*field;
});
