template<typename __char, size_t __sz, typename __func> SDK_CP_ALWAYS_INLINE void str_tokenize(__char const* const data, __char const (&ch)[__sz], __func&& f) {
    auto ptr        = data;
    auto tok_beg    = data;

    while (*ptr != 0) {
        for (auto const& c : ch) {
            if (c == *ptr) {
                if (ptr != tok_beg) {
                    f(tok_beg, ptr);
                }
                tok_beg = ptr + 1;
                break;
            }
        }
        ++ptr;
    }

    if (ptr != tok_beg) {
        f(tok_beg, ptr);
    }
}
