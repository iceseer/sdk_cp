namespace hash {

    SDK_CP_INLINE uint64_t fnv1a_hash_bytes(const uint8_t* __restrict first, uint64_t count) {
        const uint64_t fnv_offset_basis = 14695981039346656037ULL;
        const uint64_t fnv_prime = 1099511628211ULL;

        uint64_t result = fnv_offset_basis;
        for (uint64_t next = 0; next < count; ++next) {
            result ^= (uint64_t)first[next];
            result *= fnv_prime;
        }
        return (result);
    }

    SDK_CP_INLINE uint32_t fnv1a_hash_bytes_32(const uint8_t* __restrict first, size_t count) {
        const uint32_t fnv_offset_basis = 2166136261U;
        const uint32_t fnv_prime = 16777619U;

        uint32_t result = fnv_offset_basis;
        for (size_t next = 0; next < count; ++next) {
            result ^= (uint32_t)first[next];
            result *= fnv_prime;
        }
        return (result);
    }

    template<uint64_t __size> SDK_CP_INLINE void md5(uint8_t const* src, uint64_t const size, char (&out)[__size]) {
        static_assert(__size >= 2 * MD5_DIGEST_LENGTH + 1, "Buffer too small for MD5 hash.");
        uint8_t md5_hash[MD5_DIGEST_LENGTH];
        MD5(src, size, md5_hash);

        for (size_t ix = 0; ix < sizeof(md5_hash); ++ix) {
            sprintf(&out[ix * 2], "%02x", md5_hash[ix]);
        }
    }

    template<uint64_t __size> SDK_CP_INLINE void md5(std::string const& src, char(&out)[__size]) {
        md5((uint8_t const*)src.c_str(), src.size(), out);
    }
}
