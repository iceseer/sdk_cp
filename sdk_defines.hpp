#ifndef __SDK_DEFINES_HPP__
#define __SDK_DEFINES_HPP__

#define SDK_CP_UINT64_MAX 0xffffffffffffffffull
#define SDK_CP_ALIGNMENT 16
#define SDK_CP_PAGE_SIZE 4096
#define SDK_CP_CACHE_LINE 64

#ifndef SDK_CP_DEFAULT_HEAP_CHUNK
#   define SDK_CP_DEFAULT_HEAP_CHUNK 64*1024*1024
#endif//SDK_CP_DEFAULT_HEAP_CHUNK

using timestamp   = uint64_t;
using hash64_type = uint64_t;
using hash32_type = uint32_t;

timestamp const k_timeout_infinite = SDK_CP_UINT64_MAX;

#define SDK_CP_INLINE inline
#ifdef __GNUG__
#   define SDK_CP_ALWAYS_INLINE __always_inline
#elif _WIN32
#   define SDK_CP_ALWAYS_INLINE __forceinline
#endif

#ifndef SDK_CP_ARRAY_SIZE
    template <typename T, size_t N> char(&SdkCPArraySizeHelper(T(&array)[N]))[N];
#   define SDK_CP_ARRAY_SIZE(array)(sizeof(SdkCPArraySizeHelper(array)))
#endif//SDK_CP_ARRAY_SIZE

#ifndef SDK_CP_DECLARE_GETTER
#   define SDK_CP_DECLARE_GETTER(type,name,init) \
        private: \
            type _##name = init; \
        public: \
            SDK_CP_INLINE type const& name() const { \
                return field_bind(*this, _##name); \
            }
#endif//SDK_CP_DECLARE_GETTER

#ifndef SDK_CP_DECLARE_SETTER
#   define SDK_CP_DECLARE_SETTER(type,name,init) \
        private: \
            type _##name = init; \
        public: \
            SDK_CP_INLINE void name(type const& val) { \
                field_bind(*this, _##name) = val; \
            } \
            SDK_CP_INLINE void name(type&& val) { \
                field_bind(*this, _##name) = std::forward<type>(val); \
            }
#endif//SDK_CP_DECLARE_SETTER

#ifndef SDK_CP_DECLARE_GETTER_SETTER
#   define SDK_CP_DECLARE_GETTER_SETTER(type,name,init) \
        private: \
            type _##name = init; \
        public: \
            SDK_CP_INLINE void name(type&& val) { \
                field_bind(*this, _##name) = std::forward<type>(val); \
            } \
            SDK_CP_INLINE void name(type const& val) { \
                field_bind(*this, _##name) = val; \
            } \
            SDK_CP_INLINE type const& name() const { \
                return field_bind(*this, _##name); \
            }
#endif//SDK_CP_DECLARE_GETTER_SETTER

#ifndef SDK_CP_ALIGN
#   ifdef __GNUG__
#       define SDK_CP_ALIGN(type,name,al) type name __attribute__((aligned(al)))
#   elif _WIN32
#       define SDK_CP_ALIGN(type,name,al) __declspec(align(al)) type name
#   else
#       error unexpected platform
#   endif
#endif//SDK_CP_ALIGN

#ifndef SDK_CP_ALIGN_MEM
#   define SDK_CP_ALIGN_MEM(mem,base) (((mem) + static_cast<size_t>(base - 1)) & ~static_cast<size_t>(base - 1))
#endif//SDK_CP_ALIGN_MEM

#define field_bind(obj,field) (*(decltype(std::decay<decltype(obj)>::type::field)*)((uint8_t*)&obj + offsetof(std::decay<decltype(obj)>::type, field)))

template<typename T> SDK_CP_ALWAYS_INLINE T const& sdk_cp_min(T const& l, T const& r) {
    return (l < r ? l : r);
}
template<typename T> SDK_CP_ALWAYS_INLINE T const& sdk_cp_max(T const& l, T const& r) {
    return (l > r ? l : r);
}

SDK_CP_ALWAYS_INLINE size_t sdk_cp_strlen(char const* str) {
    return strlen(str);
}
SDK_CP_ALWAYS_INLINE size_t sdk_cp_strlen(wchar_t const* str) {
    return wcslen(str);
}

template<typename __char, uint32_t sz> inline __char const* r_substr_single(__char const (&data)[sz], __char const ch) {
    __char const* __restrict ptr = &data[sz];
    while (--ptr >= &data[0] && *ptr != ch);
    return ++ptr;
}

template<typename __char, uint32_t n, uint32_t m> inline __char const* r_substr(__char const (&data)[n], __char const (&chars)[m]) {
    __char const* __restrict ch = &chars[0];
    __char const* ptr = &data[0];
    do {
        ptr = sdk_cp_max(r_substr_single(data, *ch++), ptr);
    } while (ch < &chars[m]);
    return ptr;
}

inline void thread_sleep(uint64_t const usec) {
    std::this_thread::sleep_for(std::chrono::microseconds(usec));
}
inline uint64_t current_thread_id() {
#   ifdef __GNUG__
    return (uint64_t)pthread_self();
#   elif _WIN32
    return (uint64_t)GetCurrentThreadId();
#   else
#       error unexpected platform
#   endif
}

template<typename __type> void memzero(__type& t) {
    static_assert(std::is_pod<__type>::value, "__type must be pod");
    memset(&t, 0, sizeof(t));
}

template<typename __type, uint32_t __size> inline void memzero(__type (&t)[__size])
{
    static_assert(std::is_pod<__type>::value, "__type must be POD!");
    memset(&t, 0, sizeof(t));
}

template<typename __type, uint32_t N, uint32_t K> inline void memcpy_a(__type(&dst)[N], const __type(&src)[K])
{
    static_assert(std::is_pod<__type>::value, "__type must be POD!");
    static_assert(N >= K, "Array sizes must be not less.");
    memcpy(dst, src, sizeof(dst));
}

template<uint32_t __Val> struct bfs_pow2 {
    static_assert((__Val & (__Val - 1)) == 0, "__Val must be ^ 2");
    enum { count = 1 + bfs_pow2<(__Val >> 1)>::count };
};
template<> struct bfs_pow2<1> {
    enum { count = 0 };
};
template<> struct bfs_pow2<0> {
    enum { count = 0 };
};

template<int64_t __s, size_t __c> struct pow_v { enum { val = __s * pow_v<__s, __c - 1>::val }; };
template<int64_t __s>             struct pow_v<__s, 0> { enum { val = 1 }; };

template<uint64_t __digits> SDK_CP_INLINE double dbl_digits(const double in_v) {
    enum { r1 = pow_v<10, __digits + 0>::val };
    enum { r2 = pow_v<10, __digits + 1>::val };

    uint64_t const sign = (*(uint64_t*)&in_v & (1ull << 63));
    *(uint64_t*)&in_v &= ~(1ull << 63);

    uint64_t const int_p = uint64_t(in_v);
    uint64_t const fra_p = ((((in_v)-double(int_p)) * r2) + 5) / 10;

    double val = (double(int_p) + (double(fra_p) / r1));
    *(uint64_t*)&val |= sign;
    return val;
}

template<uint64_t __digits> SDK_CP_INLINE bool dbl_eq(const double d1, const double d2) {
    enum { mult = pow_v<10, __digits>::val };

    double a(d1);
    double b(d2);
    uint64_t const as = (*(uint64_t*)&a) & (1ull << 63);
    uint64_t const bs = (*(uint64_t*)&b) & (1ull << 63);
    double const r = std::numeric_limits<double>::epsilon();
    double ar = r; (*(uint64_t*)&ar) |= as; a += ar; a *= double(mult);
    double br = r; (*(uint64_t*)&br) |= bs; b += br; b *= double(mult);
    return std::abs(((as == 0) ? floor(a) : ceil(a)) -
        ((bs == 0) ? floor(b) : ceil(b))) < std::numeric_limits<double>::epsilon();
}

template<uint64_t __digits> SDK_CP_INLINE bool dbl_less(const double d1, const double d2) {
    return !dbl_eq<__digits>(d1, d2) && (d1 < d2);
}

template<uint64_t __digits> SDK_CP_INLINE bool dbl_more(const double d1, const double d2) {
    return !dbl_eq<__digits>(d1, d2) && (d1 > d2);
}

inline void debug_break() {
#   ifdef __GNUG__
        __asm__("int3");
#   elif _WIN32
        __debugbreak();
#   endif
}

#ifndef SDK_CP_ASSERT
#   ifdef SDK_CP_DEBUG
#       define SDK_CP_ASSERT(x) if (!(x)) { debug_break(); }
#   else//SDK_CP_DEBUG
#       define SDK_CP_ASSERT(x) (void)0
#   endif//SDK_CP_DEBUG
#endif//SDK_CP_ASSERT

#endif//__SDK_DEFINES_HPP__
