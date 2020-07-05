namespace processor {

    inline void pause() {
#   ifdef __GNUG__
        __asm__ ( "pause;" );
#   elif _WIN32
        _mm_pause();
#   endif
    }

    inline uint64_t bit_scan_forward_64(uint64_t const value) {
        if (0 != value) {
#   ifdef __GNUG__
            return __builtin_ctzll(value);
#   elif _WIN32
            unsigned long result;
            _BitScanForward64(&result, value);
            return result;
#   endif
        }
        return SDK_CP_UINT64_MAX;
    }

    inline void memory_barier() {
#   ifdef __GNUG__
        __sync_synchronize();
#   elif _WIN32
        MemoryBarrier();
#   endif
    }

}