template<typename __type> class objects_cache_default_allocator {
public:
    template<typename...__args> __type* allocate(__args&...args) {
        return new(std::nothrow) __type(args...);
    }
    void deallocate(__type* obj) {
        delete obj;
    }
};

template<uint32_t kMaxObjectsCount, typename T, typename __alloc = objects_cache_default_allocator<T>> class objects_cache {
    static_assert(std::is_array<T>::value == false, "array can not be used such way");

private:
    typedef std::shared_ptr<T> object_ptr;
    typedef std::deque<T*>     objects_array;

    objects_cache & operator=(const objects_cache&) = delete;
    objects_cache(const objects_cache&) = delete;

    __alloc                 _allocator;
    threading::spin_blocker _cache_blocker;
    objects_array           _cache;

    inline T* _get_raw_ptr() {
        T* ptr;
        threading::spin_blocker_infinite __lock(_cache_blocker);
        if (!_cache.empty()) {
            ptr = _cache.front();
            _cache.pop_front();
        }
        else {
            ptr = _allocator.allocate();
        }
        return ptr;
    }

    inline void _set_raw_ptr(T* const ptr) {
        if (nullptr != ptr) {
            threading::spin_blocker_infinite __lock(_cache_blocker);
            if (_cache.size() < kMaxObjectsCount) {
                _cache.push_front(ptr);
            } else {
                _allocator.deallocate(ptr);
            }
        }
    }

public:
    objects_cache() = default;
    objects_cache(__alloc& alloc) : _allocator(alloc) {

    }
    virtual ~objects_cache() {
        for (T* s : _cache) {
            _allocator.deallocate(s);
        }
    }

    T* get_cached_object() {
        return _get_raw_ptr();
    }
    void set_cached_object(T* const ptr) {
        _set_raw_ptr(ptr);
    }
    void get_cached_object(object_ptr& obj) {
        auto deleter = [this](auto* obj) {
            this->_set_raw_ptr(obj);
        };
        obj.reset(_get_raw_ptr(), deleter);
    }
};

template<uint32_t kMaxObjectsCount, typename T, typename...ARGS> class objects_cache_manager : public objects_cache<kMaxObjectsCount, T>, public objects_cache_manager<kMaxObjectsCount, ARGS...> {
public:
    virtual ~objects_cache_manager() { }
};
template<uint32_t kMaxObjectsCount, typename T> class objects_cache_manager<kMaxObjectsCount, T> : public objects_cache<kMaxObjectsCount, T> {
public:
    virtual ~objects_cache_manager() { }
};

#ifndef DECLARE_CACHE
#   define DECLARE_CACHE(prefix,size,...) \
        typedef tools::objects_cache_manager<size, __VA_ARGS__> prefix ## _cache_type; \
        extern prefix ## _cache_type prefix ## _cache; \
        template<typename T> SDK_CP_INLINE T* prefix ## _get_from_cache() { \
            return static_cast<tools::objects_cache<size, T>*>(&prefix ## _cache)->get_cached_object(); \
        } \
        template<typename T> SDK_CP_INLINE void prefix ## _set_to_cache(T* const ptr) { \
            static_cast<tools::objects_cache<size, T>*>(&prefix ## _cache)->set_cached_object(ptr); \
        } \
        template<typename T> SDK_CP_INLINE std::shared_ptr<T> prefix ## _get_shared_from_cache() { \
            std::shared_ptr<T> t; \
            static_cast<tools::objects_cache<size, T>*>(&prefix ## _cache)->get_cached_object(t); \
            return t; \
        } \
        template<typename T> SDK_CP_INLINE std::unique_ptr<T,void(*)(T* const)> prefix ## _get_unique_from_cache() { \
            return std::unique_ptr<T,void(*)(T* const)> (prefix ## _get_from_cache<T>(), &prefix ## _set_to_cache<T>); \
        }
#endif//DECLARE_CACHE

#ifndef DEFINE_EXTERN_CACHE
#   define DEFINE_EXTERN_CACHE(prefix) prefix ## _cache_type prefix ## _cache;
#endif//DEFINE_EXTERN_CACHE

#ifndef EXTRACT_SHARED_CACHE
#   define EXTRACT_SHARED_CACHE(prefix,type) prefix ## _get_shared_from_cache<type>()
#endif//EXTRACT_SHARED_CACHE

#ifndef EXTRACT_UNIQUE_CACHE
#   define EXTRACT_UNIQUE_CACHE(prefix,type) prefix ## _get_unique_from_cache<type>()
#endif//EXTRACT_UNIQUE_CACHE

#ifndef EXTRACT_RAW_CACHE
#   define EXTRACT_RAW_CACHE(prefix,type) prefix ## _get_from_cache<type>()
#endif//EXTRACT_RAW_CACHE

#ifndef INSERT_RAW_CACHE
#   define INSERT_RAW_CACHE(prefix,obj) prefix ## _set_to_cache(obj)
#endif//INSERT_RAW_CACHE

