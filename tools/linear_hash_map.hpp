template<
    typename __key,
    typename __type, 
    typename __alloc = std::allocator<__type>,
    typename __hasher = std::hash<__key>,
    typename __key_eq = std::equal_to<__key>,
    size_t __block_size = 1ull
> class linear_hash_map final {
public:
    typedef __key               key_type;
    typedef __type              value_type;
    typedef __alloc             allocator_type;
    typedef value_type&         reference_type;
    typedef value_type const&   const_reference_type;
    typedef value_type*         pointer_type;
    typedef value_type const*   const_pointer_type;
    typedef __hasher            hasher_type;
    typedef __key_eq            key_eq_type;

private:
    using map_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<std::pair<const key_type, size_t>>;

    typedef tools::pool_allocator<value_type, __block_size, allocator_type>               data_container_type;
    typedef std::unordered_map<key_type, size_t, hasher_type, key_eq_type, map_allocator> keys_container_type;

    data_container_type _data;
    keys_container_type _keys;

public:
    linear_hash_map()                                   = default;
    linear_hash_map(linear_hash_map const&)             = default;

    linear_hash_map(allocator_type const& allocator)    : _data(allocator), _keys(allocator) { }
    linear_hash_map(linear_hash_map&& c)                = default;

    linear_hash_map& operator=(linear_hash_map const&)  = default;
    linear_hash_map& operator=(linear_hash_map&& c)     = default;

public:
    reference_type operator[](key_type const& key) {
        SDK_CP_ASSERT(_data.size() == _keys.size());
        auto it = _keys.find(key);

        size_t position;
        if (_keys.end() == it) {
            position    = _data.allocate();
            _keys[key]  = position;
        } else {
            position    = it->second;
        }

        auto ptr = _data.find(position);
        SDK_CP_ASSERT(ptr != nullptr);
        return *ptr;
    }

    void erase(key_type const& key) {
        SDK_CP_ASSERT(_data.size() == _keys.size());
        auto it = _keys.find(key);
        if (_keys.end() != it) {
            _data.deallocate(it->second);
            _keys.erase(key);
        }
    }

    pointer_type find(key_type const& key) {
        SDK_CP_ASSERT(_data.size() == _keys.size());
        auto it = _keys.find(key);
        if (_keys.end() != it) {
            auto const ptr = _data.find(it->second);
            SDK_CP_ASSERT(ptr != nullptr);

            return ptr;
        }
        return nullptr;
    }

    const_pointer_type find(key_type const& key) const {
        SDK_CP_ASSERT(_data.size() == _keys.size());
        auto const it = _keys.find(key);
        if (_keys.end() != it) {
            auto const ptr = _data.find(it->second);
            SDK_CP_ASSERT(ptr != nullptr);

            return ptr;
        }
        return nullptr;
    }

    void swap(linear_hash_map& c) {
        SDK_CP_ASSERT(_data.size() == _keys.size());
        SDK_CP_ASSERT(c._data.size() == c._keys.size());
        _data.swap(c._data);
        _keys.swap(c._keys);
    }

    //void reserve(size_t const count) {
    //    SDK_CP_ASSERT(_data.size() == _keys.size());
    //    _data.reserve(count);
    //    _keys.reserve(count);
    //}

    size_t size() const {
        SDK_CP_ASSERT(_data.size() == _keys.size());
        return _data.size();
    }

    bool empty() const {
        SDK_CP_ASSERT(_data.size() == _keys.size());
        return (_data.size() == 0);
    }

    template<typename __functor_type> void foreach(__functor_type&& f) {
        SDK_CP_ASSERT(_data.size() == _keys.size());
        _data.foreach(std::forward<__functor_type>(f));
    }

    template<typename __functor_type> void foreach(__functor_type&& f) const {
        SDK_CP_ASSERT(_data.size() == _keys.size());
        _data.foreach(std::forward<__functor_type>(f));
    }
};