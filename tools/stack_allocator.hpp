///////////////////////////
// author: Alexander Lednev
// date: 03.04.2019
///////////////////////////

template<uint64_t __size> class stack_allocator final {
public:
    using pointer_type       = void*;
    using pointer_const_type = void const*;
    using point_type         = uintptr_t;

private:
    point_type _mem_ptr;
    point_type _end_ptr;
    point_type _cur_ptr;

public:
    stack_allocator() {
        _mem_ptr = (point_type)malloc(__size);
        _end_ptr = _mem_ptr + (_mem_ptr != 0 ? __size : 0);
        _cur_ptr = _mem_ptr;
    }
    ~stack_allocator() {
        free((pointer_type)_mem_ptr);
    }

    template<uint16_t __align> pointer_type allocate(uint64_t const size) {
        static_assert((__align & (__align - 1)) == 0, "__align must be pow 2.");

        auto const register ptr = SDK_CP_ALIGN_MEM(_cur_ptr, __align);
        if ((ptr + size) > _end_ptr) { return nullptr; }

        _cur_ptr = ptr + size;
        return (pointer_type)ptr;
    }

    bool contains(pointer_const_type ptr) const {
        return (ptr >= (pointer_const_type)_mem_ptr) && (ptr < (pointer_const_type)_end_ptr);
    }

    void clear() {
        _cur_ptr = _mem_ptr;
    }

    uint64_t size() const {
        return (_cur_ptr - _mem_ptr);
    }

    uint64_t capacity() const {
        return __size;
    }

    uint64_t remains() const {
        return (_end_ptr - _cur_ptr);
    }
};