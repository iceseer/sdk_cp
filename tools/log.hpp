namespace log {

#   define LOG_ENABLE_TRACE     0x01
#   define LOG_ENABLE_INFO      0x02
#   define LOG_ENABLE_WARNING   0x04
#   define LOG_ENABLE_ERROR     0x08
#   define LOG_ENABLE_ALL       0xFF

    extern uint32_t g_log_lvl;

    uint64_t const k_log_cache_size = 16;
    DECLARE_CACHE(log, k_log_cache_size, std::string);

    template<typename __t> SDK_CP_INLINE void format(std::string& buffer, __t const& t) {
        tools::str_conv(t, buffer);
    }
    template<typename __t, typename...__args> SDK_CP_INLINE void format(std::string& buffer, __t const& t, __args const&...args) {
        tools::str_conv(t, buffer);
        format(buffer, args...);
    }

    template<typename...__args> SDK_CP_INLINE void log_out(std::ostream& stream, __args const&...args) {
        auto buffer = EXTRACT_UNIQUE_CACHE(log, std::string);
        SDK_CP_ASSERT(!!buffer);

        buffer->clear();
        format(*buffer, args..., "\n");
        stream << buffer->c_str();
        stream.flush();
    }

    SDK_CP_INLINE void set_log_lvl(uint32_t const lvl) {
        g_log_lvl = lvl;
    }

    typedef struct {
        char const* log;
        char const* err;
    } log_filenames;

    SDK_CP_INLINE void set_log_filenames(log_filenames const& filenames) {
        if (nullptr != filenames.log)
            freopen(filenames.log, "a", stdout);

        if (nullptr != filenames.err)
            freopen(filenames.err, "a", stderr);
    }

    template<typename...__args> SDK_CP_INLINE void trace(__args const&...args) {
        if (g_log_lvl & LOG_ENABLE_TRACE)
#   ifdef __GNUG__
            log_out(std::cout, "\033[1;36mTRACE\033[0m ", std::chrono::system_clock::now(), " [", current_thread_id(), "] ", args...);
#   elif _WIN32
            log_out(std::cout, "TRACE ", std::chrono::system_clock::now(), " [", current_thread_id(), "] ", args...);
#   endif
    }

    template<typename...__args> SDK_CP_INLINE void info(__args const&...args) {
        if (g_log_lvl & LOG_ENABLE_INFO)
#   ifdef __GNUG__
            log_out(std::cout, "\033[1;32mINFO\033[0m ", std::chrono::system_clock::now(), " [", current_thread_id(), "] ", args...);
#   elif _WIN32
            log_out(std::cout, "INFO ", std::chrono::system_clock::now(), " [", current_thread_id(), "] ", args...);
#   endif
    }

    template<typename...__args> SDK_CP_INLINE void error(__args const&...args) {
        if (g_log_lvl & LOG_ENABLE_ERROR)
#   ifdef __GNUG__
            log_out(std::cerr, "\033[1;31mERROR\033[0m ", std::chrono::system_clock::now(), " [", current_thread_id(), "] ", args...);
#   elif _WIN32
            log_out(std::cerr, "ERROR ", std::chrono::system_clock::now(), " [", current_thread_id(), "] ", args...);
#   endif
    }

    template<typename...__args> SDK_CP_INLINE void warning(__args const&...args) {
        if (g_log_lvl & LOG_ENABLE_WARNING)
#   ifdef __GNUG__
            log_out(std::cerr, "\033[1;33mWARNING\033[0m ", std::chrono::system_clock::now(), " [", current_thread_id(), "] ", args...);
#   elif _WIN32
            log_out(std::cerr, "WARNING ", std::chrono::system_clock::now(), " [", current_thread_id(), "] ", args...);
#   endif
    }

}
