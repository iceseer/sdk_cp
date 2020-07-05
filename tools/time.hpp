template<typename __duration = std::chrono::nanoseconds> inline timestamp ts_now() {
    return std::chrono::duration_cast<__duration>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

inline timestamp time_now() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}
