#if defined(CACHE_LINE_SIZE)
inline constexpr std::size_t cache_line_size = CACHE_LINE_SIZE;
#elif defined(__cpp_lib_hardware_interference_size) && \
    (__cpp_lib_hardware_interference_size >= 201703L)
inline constexpr std::size_t cache_line_size =
    std::hardware_destructive_interference_size;
#else
    #error "cache_line_size cannot be defined"
#endif
