template<typename __type, size_t __chunk_size = 32ull, typename __alloc = std::allocator<__type>> class pool_allocator {
    template<typename __type, typename __alloc = std::allocator<__type>> class pool_chunk {
    public:
        using allocator_type    = typename __alloc::template rebind<uint8_t>::other;
        using value_type        = __type;
        using value_ptr         = value_type*;
        using const_value_ptr   = value_type const*;

    private:
        uint8_t const flag_active   = 0x01;

        size_t const flags_size     = 1;
        size_t const item_size      = sizeof(value_type) + flags_size;
        size_t const allocate_size  = __chunk_size * item_size;

        uint8_t*            _data;
        allocator_type      _def_allocator;
        allocator_type&     _alloc_obj;

        SDK_CP_ALWAYS_INLINE void _clear() {
            if (_data != nullptr) {
                auto ptr = _data;
                auto end = _data + allocate_size;

                while (ptr != end) {
                    if (flag_active & *ptr) {
                        _alloc_obj.destroy((value_ptr)(ptr + flags_size));
                    }
                    ptr += item_size;
                }

                _alloc_obj.deallocate(_data, allocate_size);
                _data = nullptr;
            }
        }

    public:
        pool_chunk() : _data(nullptr), _alloc_obj(_def_allocator) {
        }
        pool_chunk(allocator_type& alloc) : _data(nullptr), _alloc_obj(alloc) {
        }
        ~pool_chunk() {
            _clear();
        }

        pool_chunk(pool_chunk const& c)             = delete;
        pool_chunk& operator=(pool_chunk const& c)  = delete;

        pool_chunk(pool_chunk&& c) : _def_allocator(std::forward<allocator_type>(c._def_allocator)), _alloc_obj(c._alloc_obj) {
            _data = c._data;
            c._data = nullptr;
        }
        pool_chunk& operator=(pool_chunk&& c) {
            _def_allocator  = std::forward<allocator_type>(c._def_allocator);
            _alloc_obj      = c._alloc_obj;

            _clear();
            _data = c._data;
            c._data = nullptr;
        }

        bool initialize() {
            if (_data == nullptr) {
                if (_data = _alloc_obj.allocate(allocate_size)) {
                    memset(_data, 0, allocate_size);
                }
            }
            return (_data != nullptr);
        }
        template<typename...__args> void insert(size_t const position, __args&&...args) {
            SDK_CP_ASSERT(position < __chunk_size);
            auto const offset   = position * item_size;
            auto const flags    = (_data + offset);
            auto const data     = (value_ptr)(flags + flags_size);

            if (flag_active & (*flags)) {
                _alloc_obj.destroy(data);
            }

            _alloc_obj.construct(data, std::forward<__args>(args)...);
            *flags |= flag_active;
        }
        void remove(size_t const position) {
            SDK_CP_ASSERT(position < __chunk_size);
            auto const offset   = position * item_size;
            auto const flags    = (_data + offset);
            auto const data     = (value_ptr)(flags + flags_size);

            if (flag_active & (*flags)) {
                _alloc_obj.destroy(data);
            }
            *flags &= ~(flag_active);
        }
        value_ptr get_at(size_t const position) {
            SDK_CP_ASSERT(position < __chunk_size);
            auto const offset   = position * item_size;
            auto const flags    = (_data + offset);
            auto const data     = (value_ptr)(flags + flags_size);

            if (flag_active & (*flags)) {
                return data;
            }
            return nullptr;
        }
        template<typename __func> bool foreach(__func&& f) {
            auto ptr = _data;
            auto end = _data + allocate_size;

            while (ptr != end) {
                if (flag_active & *ptr) {
                    auto const data = (value_ptr)(ptr + flags_size);
                    if (!f(*data)) { return false; }
                }
                ptr += item_size;
            }
            return true;
        }
    };

public:
    using allocator_type    = __alloc;
    using value_type        = __type;
    using value_ptr         = value_type*;

private:
    using chunk_type = pool_chunk<value_type, allocator_type>;
    std::vector<chunk_type>     _data;
    std::vector<size_t>         _empty_objs;

public:
    pool_allocator(pool_allocator const&)               = delete;
    pool_allocator& operator=(pool_allocator const&)    = delete;

    pool_allocator(pool_allocator&& c) 
        : _data(std::forward<std::vector<chunk_type>>(c._data))
        , _empty_objs(std::forward<std::vector<size_t>>(c._empty_objs))
    {

    }
    pool_allocator& operator=(pool_allocator&& c) {
        _data       = std::forward<std::vector<chunk_type>>(c._data);
        _empty_objs = std::forward<std::vector<size_t>>(c._empty_objs);

        return *this;
    }

    pool_allocator() = default;
    pool_allocator(allocator_type& alloc) : _data(alloc), _empty_objs(alloc) {
    }

    template<typename...__args> size_t allocate(__args&&... args) {
        if (_empty_objs.empty()) {
            _data.emplace_back();
            auto& c = _data.back();
            if (!c.initialize()) {
                throw std::exception("Pool initialization failed.", 1);
            }

            auto const begin    = (_data.size() - 1) * __chunk_size;
            auto const end      = begin + __chunk_size;
            auto const last     = end - 1;

            _empty_objs.reserve(__chunk_size);
            for (size_t ix = begin; ix < end; ++ix) {
                _empty_objs.insert(_empty_objs.end(), last - ix + begin);
            }
        }

        auto const target       = _empty_objs.back();
        auto const chunk_pool   = (target / __chunk_size);
        auto const item_ix      = (target % __chunk_size);

        _data[chunk_pool].insert(item_ix, std::forward<__args>(args)...);
        _empty_objs.pop_back();
        return target;
    }
    void deallocate(size_t const position) {
        auto const chunk_pool   = (position / __chunk_size);
        auto const item_ix      = (position % __chunk_size);
        SDK_CP_ASSERT(chunk_pool < _data.size() && item_ix < __chunk_size);

        _data[chunk_pool].remove(item_ix);
        _empty_objs.emplace_back(position);
    }
    value_ptr find(size_t const position) {
        auto const chunk_pool = (position / __chunk_size);
        auto const item_ix    = (position % __chunk_size);
        SDK_CP_ASSERT(chunk_pool < _data.size() && item_ix < __chunk_size);

        return _data[chunk_pool].get_at(item_ix);
    }
    template<typename __func> void foreach(__func&& f) {
        for (auto& d : _data) {
            if (!d.foreach(f)) {
                break;
            }
        }
    }
    //void clear() {
    //  need to enumerate all chunks and destroy objects, but not release the memory
    //}
    size_t capacity() const {
        return (_data.size() * __chunk_size);
    }
    size_t size() const {
        return (capacity() - _empty_objs.size());
    }
    void swap(pool_allocator& c) {
        _data.swap(c._data);
        _empty_objs.swap(c._empty_objs);
    }
};
