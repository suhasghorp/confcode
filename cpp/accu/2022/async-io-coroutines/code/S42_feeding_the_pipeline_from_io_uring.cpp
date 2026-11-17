template <typename>
class move_only_function;

template <typename R, typename ... Args>
class move_only_function<R(Args...)>
{
public:
     move_only_function() = default;
     move_only_function(move_only_function&& r)
     ...
};
