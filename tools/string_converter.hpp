template<typename __string_type> SDK_CP_INLINE __string_type& str_conv(uint32_t const from, __string_type& to) {
    char buf[32];
    sprintf(buf, "%lu\0", (long unsigned int)from);

    to += buf;
    return to;
}

template<typename __string_type> SDK_CP_INLINE __string_type& str_conv(unsigned long const from, __string_type& to) {
    char buf[32];
    sprintf(buf, "%lu\0", (long unsigned int)from);

    to += buf;
    return to;
}

template<typename __string_type> SDK_CP_INLINE __string_type& str_conv(int32_t const from, __string_type& to) {
    char buf[32];
    sprintf(buf, "%d\0", (long int)from);

    to += buf;
    return to;
}

template<typename __string_type> SDK_CP_INLINE __string_type& str_conv(bool const from, __string_type& to) {
    char buf[16];
    sprintf(buf, "%s\0", from ? "true" : "false");

    to += buf;
    return to;
}

template<typename __string_type> SDK_CP_INLINE __string_type& str_conv(int64_t const from, __string_type& to) {
    char buf[32];
    sprintf(buf, "%lld\0", (long long int)from);

    to += buf;
    return to;
}

template<typename __string_type> SDK_CP_INLINE __string_type& str_conv(double const from, __string_type& to) {
    char buf[32];
    sprintf(buf, "%.8f\0", from);

    to += buf;
    return to;
}

#ifdef _WIN32
template<typename __string_type> SDK_CP_INLINE __string_type& str_conv(uint64_t const from, __string_type& to) {
    char buf[32];
    sprintf(buf, "%llu\0", (long long unsigned int)from);

    to += buf;
    return to;
}
#endif//_WIN32

inline std::string& str_conv(char const* from, std::string& to) {
    to += ((nullptr != from) ? from : "{null}");
    return to;
}

inline std::string& str_conv(char const& from, std::string& to) {
    to += from;
    return to;
}

inline std::string& str_conv(std::vector<uint8_t> const& from, std::string& to) {
    if (!from.empty()) {
        auto const * __restrict ptr       = &from[0];
        auto const * __restrict const beg = &from[0];
        auto const * __restrict const end = &from[0] + from.size();

        uint64_t const wrap_position   = 16;
        uint64_t const byte_format_len = 8;
        char const* byte_format        = " %02X('%c')";

        char     buf[512];
        uint16_t count = 0;
        while (ptr != end) {
            if (((ptr - beg) % wrap_position) == 0 && ptr != beg) {
                to.append(buf, count);
                count  = 1;
                buf[0] = '\n';
            }

            sprintf(&buf[count], byte_format, *ptr, (char)*ptr);
            count += byte_format_len;
            ++ptr;
        }
        if (from.size() % wrap_position) {
            to.append(buf, count);
        }
        to += "\nstring: ";
        to.append((char const*)beg, (char const*)end);
    }
    else {
        to += "no_data";
    }

    return to;
}

inline std::string& str_conv(std::chrono::system_clock::time_point const& from, std::string& to) {
    auto const ts = std::chrono::system_clock::to_time_t(from);
    auto const mcs = std::chrono::time_point_cast<std::chrono::microseconds>(from).time_since_epoch().count();

    char buf[64];
    using namespace std;
    auto const pos = strftime(buf, sizeof(buf), "%F %T", localtime(&ts));
    buf[pos] = '\0';

    char buf2[32];
    sprintf(buf2, ".%llu%c", (uint64_t)mcs % 1000000ull, '\0');

    to += buf;
    to += buf2;
    return to;
}

inline std::string& str_conv(std::chrono::steady_clock::time_point const& from, std::string& to) {
    auto const microsec = std::chrono::duration_cast<std::chrono::microseconds>(from.time_since_epoch());
    auto const ts  = std::chrono::duration_cast<std::chrono::seconds>(microsec).count();
    auto const mcs = microsec.count();

    char buf[64];
    using namespace std;
    auto const pos = strftime(buf, sizeof(buf), "%F %T", localtime(&ts));
    buf[pos] = '\0';

    char buf2[32];
    sprintf(buf2, ".%llu%c", (uint64_t)mcs % 1000000ull, '\0');

    to += buf;
    to += buf2;
    return to;
}

template<typename string_type> inline string_type& str_conv(std::wstring const& from, string_type& to) {
    typedef std::wstring::value_type           char_type;
    typedef std::codecvt_utf8_utf16<char_type> code_cvt;
    typedef std::allocator<char_type>          wallocator;

    std::wstring_convert<code_cvt, char_type, std::allocator<wchar_t>, typename string_type::allocator_type> converter;
    to += converter.to_bytes(from.c_str());
    return to;
}

template<typename string_type> inline string_type& str_conv(wchar_t const* from, string_type& to) {
    if (nullptr == from) {
        to += "{nullptr}";
        return to;
    }

    typedef std::wstring::value_type           char_type;
    typedef std::codecvt_utf8_utf16<char_type> code_cvt;
    typedef std::allocator<char_type>          wallocator;

    std::wstring_convert<code_cvt, char_type, std::allocator<wchar_t>, typename string_type::allocator_type> converter;
    to += converter.to_bytes(from);
    return to;
}

template<typename wstring_type> inline wstring_type& wstr_conv(uint64_t const from, wstring_type& to) {
    wchar_t buf[32];
    swprintf(buf, SDK_CP_ARRAY_SIZE(buf), L"%llu\0", (long long unsigned int)from);

    to += buf;
    return to;
}

template<typename astring_type, typename wstring_type> inline wstring_type& str_conv_to_wstr(astring_type const& from, wstring_type& to) {
    using char_type = typename wstring_type::value_type;
    using code_cvt = std::codecvt_utf8_utf16<char_type>;

    using wallocator_type = typename wstring_type::allocator_type;
    using aallocator_type = typename astring_type::allocator_type;

    std::wstring_convert<code_cvt, char_type, wallocator_type, aallocator_type> converter;
    to = converter.from_bytes(from.c_str());
    return to;
}

#ifdef _WIN32
template<typename wstring_type> inline wstring_type& str_conv_to_wstr(char const* from, wstring_type& to) {
    using char_type = typename wstring_type::value_type;
    using code_cvt = std::codecvt_utf8_utf16<char_type>;

    using wallocator_type = typename wstring_type::allocator_type;
    using aallocator_type = types::fast_allocator<char>;

    std::wstring_convert<code_cvt, char_type, wallocator_type, aallocator_type> converter;
    to = converter.from_bytes(from);
    return to;
}
#endif//_WIN32
