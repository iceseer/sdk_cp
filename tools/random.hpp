SDK_CP_INLINE uint32_t random32() {
    std::chrono::high_resolution_clock::rep const seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::ranlux48 generator(static_cast<uint32_t>(seed));
    return (uint32_t)generator();
}

SDK_CP_INLINE uint64_t random64() {
    std::chrono::high_resolution_clock::rep const seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::ranlux48_base generator(seed);
    return generator();
}

char const src_data[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+-=1234567890abcdefghijklmnopqrstuvwxyz;:'\"\\|/?.>,<";

template<size_t __sz>
SDK_CP_INLINE void random_string(uint8_t (&dst)[__sz]) {
    size_t count              = 0;
    auto const src_data_count = (sizeof(src_data) / sizeof(src_data[0])) - 1;

    while (count < __sz) {
        auto const src = random64();
        auto const rem = sdk_cp_min(__sz - count, (sizeof(src) >> 1));

        for (uint64_t i = 0; i < rem; ++i) {
            auto const sh = (i * 8);
            auto const ix = (src & (0xffull << sh)) >> sh;
            dst[count + i] = src_data[ix % src_data_count];
        }
        count += (sizeof(src) >> 1);
    }
}

SDK_CP_INLINE uint32_t mersenne_twister32() {
    auto const seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 generator((uint32_t)seed);
    return generator();
}

SDK_CP_INLINE uint64_t mersenne_twister64() {
    auto const seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 generator(seed);
    return generator();
}