#ifdef _DEBUG
#   define __USE_SERIALIZATION_CHECKS__
//#   define __PRINT_SERIALIZATION_CHECKS__
#else//_DEBUG
//#   define __USE_SERIALIZATION_CHECKS__
//#   define __PRINT_SERIALIZATION_CHECKS__
#endif//_DEBUG

#ifndef SERIALIZER_INLINE
#   define SERIALIZER_INLINE SDK_CP_ALWAYS_INLINE
#endif//SERIALIZER_INLINE

#ifndef SERIALIZER_COMPACT_STR_ARRAYS
//#   define SERIALIZER_COMPACT_STR_ARRAYS
#endif//SERIALIZER_COMPACT_STR_ARRAYS

#ifndef SERIALIZER_ASSERT
#   define SERIALIZER_ASSERT(x) SDK_CP_ASSERT(x)
#endif//SERIALIZER_ASSERT

#ifdef __USE_SERIALIZATION_CHECKS__
#   define DEFINE_INIT_SIZE const auto __Was = size;
#   define CHECK_BEC_SIZE(uniq,type,name) { \
        const auto __Become = size; \
        PRINT_CHECK(#uniq ": " #type "." #name " was serialized in %lu\n", __Was - __Become); \
        if (__Was - __Become != binary_serialization::amtsSize(t.name)) { \
            PRINT_CHECK(#uniq ": " #type "." #name " calculated [%lu], offsets [%lu]\n", binary_serialization::amtsSize(t.name), __Was - __Become); \
            __debugbreak(); \
        } \
    }
#   define CHECK_PRINT(uniq,type,name,val) PRINT_CHECK(#uniq ": " #type "." #name " was calculated %lu\n", val);
#else//__USE_SERIALIZATION_CHECKS__
#   define DEFINE_INIT_SIZE 
#   define CHECK_BEC_SIZE(uniq,type,name)
#   define CHECK_PRINT(uniq,type,name,val)
#endif//__USE_SERIALIZATION_CHECKS__

#define SFINAE_DECLARE_MEMBER(parent,type,name) \
template<typename T> struct __sfiname_has_mem_ ## parent ## name { \
    struct Fallback { type name; }; \
    struct Derived : T, Fallback { }; \
    template<typename C, C> struct ChT; \
    template<typename C> static char(&f(ChT<type Fallback::*, &C::name>*))[1]; \
    template<typename C> static char(&f(...))[2]; \
    static bool const value = sizeof(f<Derived>(0)) == 2; \
};

#define SFINAE_DECLARE_0(method) \
template<typename T> class __sfinae_has_ ## method { \
private: \
    static int detect(...); \
    template<typename U> static decltype(std::declval<U>().method()) detect(const U&); \
public: \
    enum { value = std::is_same<void, decltype(detect(std::declval<T>()))>::value }; \
};

#define SFINAE_DECLARE_1(method,param1) \
template<typename T> class __sfinae_has_ ## method { \
private: \
    static int detect(...); \
    template<typename U> static decltype(std::declval<U>().method(param1())) detect(const U&); \
public: \
    enum { value = std::is_same<void, decltype(detect(std::declval<T>()))>::value }; \
};

#define SFINAE_DECLARE_2(method,param1,param2) \
template<typename T> class __sfinae_has_ ## method { \
private: \
    static int detect(...); \
    template<typename U> static decltype(std::declval<U>().method(param1(),param2())) detect(const U&); \
public: \
    enum { value = std::is_same<void, decltype(detect(std::declval<T>()))>::value }; \
};

#define SFINAE_DECLARE_3(method,param1,param2,param3) \
template<typename T> class __sfinae_has_ ## method { \
private: \
    static int detect(...); \
    template<typename U> static decltype(std::declval<U>().method(param1(),param2(),param3())) detect(const U&); \
public: \
    enum { value = std::is_same<void, decltype(detect(std::declval<T>()))>::value }; \
};

#define SFINAE_CHECK(method,type) __sfinae_has_ ## method<type>::value
#define SFINAE_CHECK_MEMBER(unique,parent,type,name) __sfiname_has_mem_ ## unique ## name<parent>::value

#ifdef __PRINT_SERIALIZATION_CHECKS__
#   define PRINT_CHECK(...) printf(__VA_ARGS__);
#else//__PRINT_SERIALIZATION_CHECKS__
#   define PRINT_CHECK(...)
#endif//__PRINT_SERIALIZATION_CHECKS__

#ifndef DECLARE_SERIALIZABLE_MEMBER
#   define DECLARE_SERIALIZABLE_MEMBER(unique,type,name) \
        SFINAE_DECLARE_MEMBER(unique, type, name); \
        namespace _internal ## unique ## name{ \
            template<int enabled> struct InternalSerialize \
            { \
                template<typename __Type> \
                static SERIALIZER_INLINE bool proc(__Type& t, uint8_t*& buffer, uint32_t& size) \
                { \
                    bool res = false; \
                    DEFINE_INIT_SIZE; \
                    res = binary_serialization::amtsSerialize(t.name, buffer, size); \
                    CHECK_BEC_SIZE(unique,type,name); \
                    return res; \
                } \
            }; \
            template<> struct InternalSerialize<0> \
            { \
                template<typename __Type> \
                static SERIALIZER_INLINE bool proc(__Type& /*t*/, uint8_t*& /*buffer*/, uint32_t& /*size*/) \
                { \
                    SERIALIZER_ASSERT(!"Unexpected serialize routine!"); \
                    return false; \
                } \
            }; \
            template<int enabled> struct InternalDeserialize \
            { \
                template<typename __Type> \
                static SERIALIZER_INLINE bool proc(__Type& t, const uint8_t*& buffer, uint32_t& size) \
                { \
                    return binary_serialization::amtsDeserialize(t.name, buffer, size); \
                } \
            }; \
            template<> struct InternalDeserialize<0> \
            { \
                template<typename __Type> \
                static SERIALIZER_INLINE bool proc(__Type& /*t*/, const uint8_t*& /*buffer*/, uint32_t& /*size*/) \
                { \
                    SERIALIZER_ASSERT(!"Unexpected deserialize routine!"); \
                    return false; \
                } \
            }; \
            template<int enabled> struct InternalSize \
            { \
                template<typename __Type> \
                static SERIALIZER_INLINE uint32_t proc(__Type& t) \
                { \
                    const auto res = binary_serialization::amtsSize(t.name); \
                    CHECK_PRINT(unique,type,name,res); \
                    return res; \
                } \
            }; \
            template<> struct InternalSize<0> \
            { \
                template<typename __Type> \
                static SERIALIZER_INLINE uint32_t proc(__Type& /*t*/) \
                { \
                    SERIALIZER_ASSERT(!"Unexpected size routine!"); \
                    return false; \
                } \
            }; \
            template<int enabled> struct InternalMove \
            { \
                template<typename __Type> \
                static SERIALIZER_INLINE bool proc(__Type& src, __Type& dst) \
                { \
                    return binary_serialization::amtsMove(src.name, dst.name); \
                } \
            }; \
            template<> struct InternalMove<0> \
            { \
                template<typename __Type> \
                static SERIALIZER_INLINE bool proc(__Type& /*src*/, __Type& /*dst*/) \
                { \
                    SERIALIZER_ASSERT(!"Unexpected move routine!"); \
                    return false; \
                } \
            }; \
        }
#endif//DECLARE_SERIALIZABLE_MEMBER

#ifndef _INTERNAL_SERIALIZE
#   define _INTERNAL_SERIALIZE(unique,parent,type,name) _internal ## unique ## name::InternalSerialize< SFINAE_CHECK_MEMBER(unique, parent, type, name) >::proc(t, buffer, size)
#endif//_INTERNAL_SERIALIZE

#ifndef _INTERNAL_DESERIALIZE
#   define _INTERNAL_DESERIALIZE(unique,parent,type,name) _internal ## unique ## name::InternalDeserialize< SFINAE_CHECK_MEMBER(unique, parent, type, name) >::proc(t, buffer, size)
#endif//_INTERNAL_DESERIALIZE

#ifndef _INTERNAL_SIZE
#   define _INTERNAL_SIZE(unique,parent,type,name) _internal ## unique ## name::InternalSize< SFINAE_CHECK_MEMBER(unique, parent, type, name) >::proc(t)
#endif//_INTERNAL_SIZE

#ifndef _INTERNAL_MOVE
#   define _INTERNAL_MOVE(unique,parent,type,name) _internal ## unique ## name::InternalMove< SFINAE_CHECK_MEMBER(unique, parent, type, name) >::proc(src, dst)
#endif//_INTERNAL_MOVE

#define DEFAULT_IMPLEMENTATION(type) \
            template<int32_t __Maj, int32_t __Min>  struct Serializer \
            { \
                SERIALIZER_INLINE static bool proc(type& t, uint8_t*& buffer, uint32_t& size) \
                { \
                    static_assert(__Min >= 0, __FUNCTION__ " is not defined for this version for type " #type ); \
                    return Serializer<__Maj, __Min - 1>::proc(t, buffer, size); \
                } \
            }; \
            template<int32_t __Maj, int32_t __Min>  struct Deserializer \
            { \
                SERIALIZER_INLINE static bool proc(type& t, const uint8_t*& buffer, uint32_t& size) \
                { \
                    static_assert(__Min >= 0, __FUNCTION__ " is not defined for this version for type " #type ); \
                    return Deserializer<__Maj, __Min - 1>::proc(t, buffer, size); \
                } \
            }; \
            template<int32_t __Maj, int32_t __Min>  struct Move \
            { \
                SERIALIZER_INLINE static bool proc(type& src, type& dst) \
                { \
                    static_assert(__Min >= 0, __FUNCTION__ " is not defined for this version for type " #type ); \
                    return Move<__Maj, __Min - 1>::proc(src, dst); \
                } \
            }; \
            template<int32_t __Maj, int32_t __Min> struct Size \
            { \
                SERIALIZER_INLINE static uint32_t proc(type& t) \
                { \
                    static_assert(__Min >= 0, __FUNCTION__ " is not defined for this version for type " #type ); \
                    return Size<__Maj, __Min - 1>::proc(t); \
                } \
            };

namespace binary_serialization {

    template<typename __Type> SERIALIZER_INLINE bool amtsSerialize(__Type& t, uint8_t*& buffer, uint32_t& size);
    template<typename __Type> SERIALIZER_INLINE bool amtsSerialize(std::vector<__Type>& t, uint8_t*& buffer, uint32_t& size);
    template<uint32_t __Sz>   SERIALIZER_INLINE bool amtsSerialize(wchar_t(&t)[__Sz], uint8_t*& buffer, uint32_t& size);
    template<typename __Type> SERIALIZER_INLINE bool amtsSerialize(std::basic_string<__Type>& t, uint8_t*& buffer, uint32_t& size);

    template<typename __Type> SERIALIZER_INLINE uint32_t amtsSize(__Type& t);
    template<typename __Type> SERIALIZER_INLINE uint32_t amtsSize(std::vector<__Type>& t);
    template<uint32_t __Sz>   SERIALIZER_INLINE uint32_t amtsSize(wchar_t(&t)[__Sz]);
    template<typename __Type> SERIALIZER_INLINE uint32_t amtsSize(std::basic_string<__Type>& t);

    template<typename __Type> SERIALIZER_INLINE bool amtsDeserialize(__Type& t, const uint8_t*& buffer, uint32_t& size);
    template<typename __Type> SERIALIZER_INLINE bool amtsDeserialize(std::vector<__Type>& t, const uint8_t*& buffer, uint32_t& size);
    template<uint32_t __Sz>   SERIALIZER_INLINE bool amtsDeserialize(wchar_t(&t)[__Sz], const uint8_t*& buffer, uint32_t& size);
    template<typename __Type> SERIALIZER_INLINE bool amtsDeserialize(std::basic_string<__Type>& t, const uint8_t*& buffer, uint32_t& size);

    template<typename __Type> SERIALIZER_INLINE bool amtsMove(__Type& src, __Type& dst);
    template<typename __Type> SERIALIZER_INLINE bool amtsMove(std::vector<__Type>& src, std::vector<__Type>& dst);
    template<uint32_t __Sz>   SERIALIZER_INLINE bool amtsMove(wchar_t(&src)[__Sz], wchar_t(&dst)[__Sz]);
    template<typename __Type> SERIALIZER_INLINE bool amtsMove(std::basic_string<__Type>& src, std::basic_string<__Type>& dst);

    template<typename C, typename __Type> SERIALIZER_INLINE bool     __amtsSerialize(C& t, uint8_t*& buffer, uint32_t& size);
    template<typename C, typename __Type> SERIALIZER_INLINE uint32_t __amtsSize(C& t);
    template<typename C, typename __Type> SERIALIZER_INLINE bool     __amtsDeserialize(C& t, const uint8_t*& buffer, uint32_t& size);
    template<typename C, typename __Type> SERIALIZER_INLINE bool     __amtsMove(C& src, C& dst);

    template<typename __Type, uint32_t __IsPOD> struct Core
    {
        template<int32_t __Maj, int32_t __Min> struct Serializer
        {
            SERIALIZER_INLINE static bool proc(__Type& t, uint8_t*& buffer, uint32_t& size)
            {
                static_assert(false, __FUNCTION__ " is not defined for type.");
                return false;
            }
        };

        template<int32_t __Maj, int32_t __Min> struct Deserializer
        {
            SERIALIZER_INLINE static bool proc(__Type& t, const uint8_t*& buffer, uint32_t& size)
            {
                static_assert(false, __FUNCTION__ " is not defined for type.");
                return false;
            }
        };

        template<int32_t __Maj, int32_t __Min> struct Move
        {
            SERIALIZER_INLINE static bool proc(__Type& src, __Type& dst)
            {
                static_assert(false, __FUNCTION__ " is not defined for type.");
                return false;
            }
        };

        template<int32_t __Maj, int32_t __Min> struct Size
        {
            SERIALIZER_INLINE static uint32_t proc(__Type& t)
            {
                static_assert(false, __FUNCTION__ " is not defined for type.");
                return false;
            }
        };
    };

    template<typename __Type> struct Core<__Type, 1>
    {
        template<int32_t __Maj, int32_t __Min> struct Serializer
        {
            SERIALIZER_INLINE static bool proc(__Type& t, uint8_t*& buffer, uint32_t& size)
            {
                const uint32_t typeSz = amtsSize(t);
                if (size >= typeSz) {
                    ::memcpy(buffer, &t, typeSz);
                    buffer += typeSz;
                    size -= typeSz;
                    return true;
                }
                SERIALIZER_ASSERT(!"Too less buffer size for serialization!");
                return false;
            }
        };

        template<int32_t __Maj, int32_t __Min> struct Deserializer
        {
            SERIALIZER_INLINE static bool proc(__Type& t, const uint8_t*& buffer, uint32_t& size)
            {
                const uint32_t typeSz = amtsSize(t);
                if (size >= typeSz) {
                    ::memcpy(&t, buffer, typeSz);
                    buffer += typeSz;
                    size -= typeSz;
                    return true;
                }
                return false;
            }
        };

        template<int32_t __Maj, int32_t __Min> struct Move
        {
            SERIALIZER_INLINE static bool proc(__Type& src, __Type& dst)
            {
                ::memcpy(&dst, &src, amtsSize(dst));
                return true;
            }
        };

        template<int32_t __Maj, int32_t __Min> struct Size
        {
            SERIALIZER_INLINE static uint32_t proc(__Type& /*t*/)
            {
                return sizeof(__Type);
            }
        };
    };

    template<typename __Type> SERIALIZER_INLINE bool amtsSerialize(__Type& t, uint8_t*& buffer, uint32_t& size)
    {
        SERIALIZER_ASSERT(buffer != nullptr);
        SERIALIZER_ASSERT(size >= amtsSize(t));

        return Core<__Type, std::is_pod<__Type>::value>::Serializer<API_VERSION_MAJOR, API_VERSION_MINOR>::proc(t, buffer, size);
    }
    template<uint32_t __Sz> SERIALIZER_INLINE bool amtsSerialize(wchar_t(&t)[__Sz], uint8_t*& buffer, uint32_t& size)
    {
        SERIALIZER_ASSERT(buffer != nullptr);

#ifdef SERIALIZER_COMPACT_STR_ARRAYS
        if (size >= amtsSize(t)) {
            uint32_t count = wcsnlen(t, __Sz);
            uint32_t sz = count * sizeof(wchar_t);
            if (amtsSerialize(count, buffer, size)) {
                ::memcpy(buffer, t, sz);
                buffer += sz;
                size -= sz;
                return true;
            }
        }
#else//SERIALIZER_COMPACT_STR_ARRAYS
        if (size >= amtsSize(t)) {
            ::memcpy(buffer, t, sizeof(t));
            buffer += sizeof(t);
            size -= sizeof(t);
            return true;
        }
#endif//SERIALIZER_COMPACT_STR_ARRAYS

        SERIALIZER_ASSERT(!"Serialization failed");
        return false;
    }
    template<typename C, typename __Type> SERIALIZER_INLINE bool __amtsSerialize(C& t, uint8_t*& buffer, uint32_t& size)
    {
        SERIALIZER_ASSERT(buffer != nullptr);
        SERIALIZER_ASSERT(size >= amtsSize(t));

        const uint64_t sz = amtsSize(t);
        if (sz > size) {
            return false;
        }

        uint64_t count = (uint64_t)t.size();
        if (!amtsSerialize(count, buffer, size)) {
            return false;
        }

        for (size_t ix = 0; ix < t.size(); ++ix) {
            if (!amtsSerialize(t[ix], buffer, size)) {
                return false;
            }
        }
        return true;
    }
    template<typename __Type> SERIALIZER_INLINE bool amtsSerialize(std::vector<__Type>& t, uint8_t*& buffer, uint32_t& size) {
        return __amtsSerialize<std::vector<__Type>, __Type>(t, buffer, size);
    }
    template<typename __Type> SERIALIZER_INLINE bool amtsSerialize(std::basic_string<__Type>& t, uint8_t*& buffer, uint32_t& size) {
        SERIALIZER_ASSERT(buffer != nullptr);
        SERIALIZER_ASSERT(size >= amtsSize(t));
        uint64_t count = (uint64_t)t.size();
        if (!amtsSerialize(count, buffer, size)) return false;
        const uint64_t sz = count * sizeof(__Type);
        if (sz > size) return false;
        if (sz > 0) {
            ::memcpy(buffer, &t.front(), sz);
            buffer += sz;
            size -= sz;
        }
        return true;
    }

    template<uint32_t __Sz> SERIALIZER_INLINE uint32_t amtsSize(wchar_t(&t)[__Sz])
    {
#ifdef SERIALIZER_COMPACT_STR_ARRAYS
        uint32_t const count = wcsnlen(t, __Sz);
        SERIALIZER_ASSERT(count < __Sz);

        return sizeof(uint32_t) + (count * sizeof(wchar_t));
#else//SERIALIZER_COMPACT_STR_ARRAYS
        return sizeof(t);
#endif//SERIALIZER_COMPACT_STR_ARRAYS
    }
    template<typename __Type> SERIALIZER_INLINE uint32_t amtsSize(__Type& t)
    {
        return Core<__Type, std::is_pod<__Type>::value>::Size<API_VERSION_MAJOR, API_VERSION_MINOR>::proc(t);
    }
    template<typename C, typename __Type> SERIALIZER_INLINE uint32_t __amtsSize(C& t)
    {
        const uint64_t count = (uint64_t)t.size();
        uint64_t sz = amtsSize(count);
        for (size_t ix = 0; ix < t.size(); ++ix) {
            sz += amtsSize(t[ix]);
        }
        return sz;
    }
    template<typename __Type> SERIALIZER_INLINE uint32_t amtsSize(std::vector<__Type>& t) {
        return __amtsSize<std::vector<__Type>, __Type>(t);
    }
    template<typename __Type> SERIALIZER_INLINE uint32_t amtsSize(std::basic_string<__Type>& t) {
        auto length = static_cast<uint64_t>(t.size());
        return amtsSize(length) + length * sizeof(__Type);
    }

    template<uint32_t __Sz> SERIALIZER_INLINE bool amtsDeserialize(wchar_t(&t)[__Sz], const uint8_t*& buffer, uint32_t& size)
    {
        SERIALIZER_ASSERT(buffer != nullptr);

#ifdef SERIALIZER_COMPACT_STR_ARRAYS
        uint32_t count;
        if (amtsDeserialize(count, buffer, size) && count <= __Sz) {
            const uint32_t sz = count * sizeof(wchar_t);
            ::memcpy(t, buffer, sz);
            t[count] = 0;

            buffer += sz;
            size -= sz;
            return true;
        }
#else//SERIALIZER_COMPACT_STR_ARRAYS
        if (size >= __Sz) {
            ::memcpy(t, buffer, sizeof(t));
            buffer += sizeof(t);
            size -= sizeof(t);
            return true;
        }
#endif//SERIALIZER_COMPACT_STR_ARRAYS

        SERIALIZER_ASSERT(!"Deserialization failed");
        return false;
    }
    template<typename __Type> SERIALIZER_INLINE bool amtsDeserialize(__Type& t, const uint8_t*& buffer, uint32_t& size)
    {
        SERIALIZER_ASSERT(buffer != nullptr);
        return Core<__Type, std::is_pod<__Type>::value>::Deserializer<API_VERSION_MAJOR, API_VERSION_MINOR>::proc(t, buffer, size);
    }
    template<typename C, typename __Type> SERIALIZER_INLINE bool __amtsDeserialize(C& t, const uint8_t*& buffer, uint32_t& size)
    {
        try {
            uint64_t count;
            if (size < amtsSize(count) || !amtsDeserialize(count, buffer, size)) {
                return false;
            }

            t.resize(count);
            for (uint64_t ix = 0; ix < count; ++ix) {
                if (!amtsDeserialize(t[ix], buffer, size)) {
                    return false;
                }
            }

            return true;
        }
        catch (std::exception&) {}

        SERIALIZER_ASSERT(!"Exception: " __FUNCTION__);
        return false;
    }
    template<typename __Type> SERIALIZER_INLINE bool amtsDeserialize(std::vector<__Type>& t, const uint8_t*& buffer, uint32_t& size) {
        return __amtsDeserialize<std::vector<__Type>, __Type>(t, buffer, size);
    }
    template<typename __Type> SERIALIZER_INLINE bool amtsDeserialize(std::basic_string<__Type>& t, const uint8_t*& buffer, uint32_t& size) {
        try {
            uint64_t count;
            if (size < amtsSize(count) || !amtsDeserialize(count, buffer, size)) return false;
            const uint64_t sz = count * sizeof(__Type);
            if (size < sz) return false;
            if (sz > 0) {
                t.resize(count);
                ::memcpy(&t.front(), buffer, sz);
                buffer += sz;
                size -= sz;
            }
            return true;
        }
        catch (std::exception&) {}
        SERIALIZER_ASSERT(!"Exception: " __FUNCTION__);
        return false;
    }

    template<typename __Type> SERIALIZER_INLINE bool amtsMove(__Type& src, __Type& dst)
    {
        return Core<__Type, std::is_pod<__Type>::value>::Move<API_VERSION_MAJOR, API_VERSION_MINOR>::proc(src, dst);
    }
    template<uint32_t __Sz> SERIALIZER_INLINE bool amtsMove(wchar_t(&src)[__Sz], wchar_t(&dst)[__Sz])
    {
        ::memcpy(dst, src, sizeof(dst));
        return true;
    }
    template<typename C, typename __Type> SERIALIZER_INLINE bool __amtsMove(C& src, C& dst)
    {
        try {
            dst.swap(src);
            return true;
        }
        catch (std::exception&) {}

        SERIALIZER_ASSERT(!"Exception: " __FUNCTION__);
        return false;
    }
    template<typename __Type> SERIALIZER_INLINE bool amtsMove(std::vector<__Type>& src, std::vector<__Type>& dst) {
        return __amtsMove<std::vector<__Type>, __Type>(src, dst);
    }
    template<typename __Type> SERIALIZER_INLINE bool amtsMove(std::basic_string<__Type>& src, std::basic_string<__Type>& dst) {
        try {
            dst = std::move(src);
            return true;
        }
        catch (std::exception&) {}
        SERIALIZER_ASSERT(!"Exception: " __FUNCTION__);
        return false;
    }

}