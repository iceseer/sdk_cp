//======================================
//========== Black Box Studio ==========
//====== author: Alexander Lednev ======
//========== date: 20.03.2017 ==========
//======================================
/*
    *** Fast ring buffer
    *** functionality: allocate, skip, commit, release, peek
*/

#ifdef SDK_CP_DEBUG
#   define __USE_GUARD__
#endif//SDK_CP_DEBUG

#ifndef LOCK_ASSERT
#   define LOCK_ASSERT(x) SDK_CP_ASSERT(x)
#endif//LOCK_ASSERT

namespace containers {

    template<uint32_t __size, uint32_t __alignment = SDK_CP_ALIGNMENT> class ring_buffer final {
        static_assert((__alignment > 0),                      "__alignment must be more than 0");
        static_assert((__alignment <= 128),                   "__alignment must be less than 128");
        static_assert((__alignment & (__alignment - 1)) == 0, "__alignment must be ^ 2");

        static_assert((__size >= 64),                         "Buffer size must be more than 96b");
        static_assert((__size & (__size - 1)) == 0,           "Buffer size must be ^ 2");

        enum { k_size_bound          = bfs_pow2<__size>::count };
        enum { k_guard               = 0xdeadbeaf };
        enum { k_slot_alignment      = __alignment };
        enum { k_slot_bound          = bfs_pow2<k_slot_alignment>::count };
        enum { k_slot_alignment_mask = (k_slot_alignment - 1) };
        enum { k_cache_line          = SDK_CP_CACHE_LINE };

        static_assert((k_slot_alignment == (1u << k_slot_bound)), "Logic error in alignment calculation");
        typedef unsigned long load_ptr_t;

        static_assert(sizeof(size_t) == 8, "this code support only x64");
#ifdef __USE_GUARD__
        struct prefix {
            enum { busy = 0, fake = 1, commited = 2, skipped = 3 };

            uint32_t guard;
            uint32_t size;
            uint16_t state;

            //~~~~~~~~~~~~~~
            enum { k_without_padding_size = 2 * sizeof(uint32_t) + sizeof(uint16_t) };
            enum { k_padding_size = ((k_without_padding_size >> k_slot_bound) + ((k_without_padding_size & k_slot_alignment_mask) ? 1 : 0)) * k_slot_alignment };
            uint8_t __padding__[k_padding_size - k_without_padding_size];
        };
        static_assert((sizeof(prefix) & k_slot_alignment_mask) == 0, "Padding is incorrect");
#else//__USE_GUARD__
        struct prefix {
            enum { busy = 0, fake = 1, commited = 2, skipped = 3 };

            uint32_t size;
            uint16_t state;

            //~~~~~~~~~~~~~~
            enum { k_without_padding_size = sizeof(uint16_t) + sizeof(uint32_t) };
            enum { k_padding_size = ((k_without_padding_size >> k_slot_bound) + ((k_without_padding_size & k_slot_alignment_mask) ? 1 : 0)) * k_slot_alignment };
            uint8_t __padding__[k_padding_size - k_without_padding_size];
        };
        static_assert((sizeof(prefix) & k_slot_alignment_mask) == 0, "Padding is incorrect");
#endif//__USE_GUARD__


#ifdef __USE_GUARD__
        struct postfix {
            uint32_t guard;

            //~~~~~~~~~~~~~~
            enum { k_without_padding_size = sizeof(uint32_t) };
            enum { k_padding_size = ((k_without_padding_size >> k_slot_bound) + ((k_without_padding_size & k_slot_alignment_mask) ? 1 : 0)) * k_slot_alignment };
            uint8_t __padding__[k_padding_size - k_without_padding_size];
        };
        static_assert((sizeof(postfix) & k_slot_alignment_mask) == 0, "Padding is incorrect");
#endif//__USE_GUARD__


#ifdef __USE_GUARD__
        enum { k_overhead = sizeof(prefix) + sizeof(postfix) };
#else//__USE_GUARD__
        enum { k_overhead = sizeof(prefix) };
#endif//__USE_GUARD__
        static_assert((k_overhead & k_slot_alignment_mask) == 0, "Overhead structures padding is incorrect");


        std::vector<uint8_t> _mem_raw_ptr;
        uint8_t* _mem_ptr;

        std::atomic<load_ptr_t> _ix_read;
        std::atomic<load_ptr_t> _ix_write;

        threading::spin_blocker _write_blocker;

    public:
        typedef uint32_t handle_t;
        const handle_t k_invalid_handle = (handle_t)-1;

    private:
        void _update_block_state(handle_t const handle, uint16_t const state) {
            if (handle != k_invalid_handle) {
                uint8_t* const begPtr = _mem_ptr + (handle & (__size - 1));

                prefix* const pr = (prefix*)begPtr;
                pr->state = state;

#ifdef __USE_GUARD__
                tools::processor::memory_barier();
                LOCK_ASSERT(pr->guard == k_guard);
#endif//__USE_GUARD__

#ifdef __USE_GUARD__
                postfix* const pst = (postfix*)(begPtr + sizeof(prefix) + pr->size);
                LOCK_ASSERT(pst->guard == k_guard);
#endif//__USE_GUARD__

                tools::processor::memory_barier();
            }
        }

        uint8_t* _get_first_block(load_ptr_t& readIx, uint32_t& size) {
            auto* ptr  = _mem_ptr + (readIx & (__size - 1));
            prefix* pr = (prefix*)ptr;

#ifdef __USE_GUARD__
            LOCK_ASSERT(pr->guard == k_guard);
#endif//__USE_GUARD__

            if (pr->state == prefix::fake) {
                readIx += pr->size;
            }
            else {
                while (pr->state == prefix::busy) { thread_sleep(1); }
                readIx += k_overhead + pr->size;

#ifdef __USE_GUARD__
                postfix* pst = (postfix*)(ptr + sizeof(prefix) + pr->size);
                LOCK_ASSERT(pst->guard == k_guard);
#endif//__USE_GUARD__

                if (pr->state == prefix::commited) {
                    size = pr->size;
                    return ptr + sizeof(prefix);
                }
            }
            return nullptr;
        }

    public:
        ring_buffer(ring_buffer const&) = delete;
        ring_buffer& operator=(ring_buffer const&) = delete;

        ring_buffer() : _ix_read(ATOMIC_VAR_INIT(0)), _ix_write(ATOMIC_VAR_INIT(0)) {
            _mem_raw_ptr.resize(__size + (k_cache_line - 1));
            _mem_ptr = &_mem_raw_ptr[0] + ((k_cache_line - 1) & (k_cache_line - (uintptr_t(&_mem_raw_ptr[0]) & (k_cache_line - 1))));
        }

        uint8_t* allocate(uint32_t size, handle_t& handle, bool const no_wait) {
            handle = k_invalid_handle;
            if (size == 0 || size > (__size >> 2)) {
                return nullptr;
            }
            threading::spin_blocker_infinite write_lock(_write_blocker);

            size += k_slot_alignment_mask & (k_slot_alignment - (size & k_slot_alignment_mask));
            const auto data_size = size;
            size += k_overhead;

            load_ptr_t commit_size  = size;
            load_ptr_t write_beg_ix = _ix_write;
            load_ptr_t write_end_ix = write_beg_ix + commit_size;

            const load_ptr_t end_block = (write_end_ix >> k_size_bound) - (1 & (!(write_end_ix & (__size - 1))));
            const load_ptr_t beg_block = (write_beg_ix >> k_size_bound);
            uint32_t bufferPos = write_beg_ix & (__size - 1);

            auto* ptr = &_mem_ptr[bufferPos];
            if (end_block != beg_block) {
                const load_ptr_t border = write_beg_ix + sizeof(prefix);
                if (no_wait) {
                    if (border - _ix_read > __size) {
                        return nullptr;
                    }
                } else {
                    while (border - _ix_read > __size) {
                        thread_sleep(1);
                    }
                }

                const load_ptr_t remains = __size - bufferPos;
                prefix* const pr = (prefix*)ptr;

#ifdef __USE_GUARD__
                pr->guard = k_guard;
#endif//__USE_GUARD__

                pr->size  = remains;
                pr->state = prefix::fake;

                write_end_ix += remains;
                write_beg_ix += remains;
                commit_size  += remains;

                bufferPos = 0;
                ptr       = &_mem_ptr[0];
            }

            if (no_wait) {
                if (write_end_ix - _ix_read > __size) {
                    return nullptr;
                }
            } else {
                while (write_end_ix - _ix_read > __size) {
                    thread_sleep(1);
                }
            }

            prefix* const pr = (prefix*)ptr;
            pr->size         = data_size;
            pr->state        = prefix::busy;
#ifdef __USE_GUARD__
            pr->guard        = k_guard;
#endif//__USE_GUARD__

            uint8_t* const ptr_data = ptr + sizeof(prefix);
#ifdef __USE_GUARD__
            auto* const pst = (postfix*)(ptr_data + data_size);
            pst->guard = k_guard;
#endif//__USE_GUARD__

            _ix_write += commit_size;

            handle = write_beg_ix;
            LOCK_ASSERT((_mem_ptr + (handle & (__size - 1))) == ptr);

            return ptr_data;
        }

        void commit(handle_t const handle) {
            _update_block_state(handle, prefix::commited);
        }

        void skip(handle_t const handle) {
            _update_block_state(handle, prefix::skipped);
        }

        uint8_t* peek(uint32_t& size)
        {
            load_ptr_t readIx = _ix_read;
            while(true) {
                if (_ix_write == readIx) {
                    const auto difference = readIx - _ix_read;
                    if (difference > 0) {
                        _ix_read += difference;
                    }
                    return nullptr;
                }

                if (auto* ret_ptr = _get_first_block(readIx, size)) {
                    return ret_ptr;
                }
            }
        }

        void free()
        {
            load_ptr_t readIx = _ix_read;
            while(true) {
                while (_ix_write == readIx) { thread_sleep(1); }

                uint32_t size;
                if (nullptr != _get_first_block(readIx, size)) {
                    break;
                }
            }
            _ix_read += readIx - _ix_read;
        }
    };

    template<typename __buffer> class buffer_auto_commit final {
        buffer_auto_commit(const buffer_auto_commit&);
        buffer_auto_commit& operator=(buffer_auto_commit&);

        __buffer* _buffer;
        uint32_t  _handle;

    public:
        buffer_auto_commit(__buffer& buffer) : _buffer(&buffer), _handle(uint32_t(-1)) {

        }
        buffer_auto_commit() : _buffer(nullptr), _handle(uint32_t(-1)) {

        }

        buffer_auto_commit(buffer_auto_commit&& c) : _buffer(c._buffer), _handle(c._handle) {
            c._buffer = nullptr;
            c._handle = uint32_t(-1);
        }

        buffer_auto_commit& operator=(buffer_auto_commit&& c) {
            if (nullptr != _buffer) {
                _buffer->commit(_handle);
            }

            _buffer = c._buffer;
            _handle = c._handle;

            c._buffer = nullptr;
            c._handle = uint32_t(-1);
        }

        ~buffer_auto_commit() {
            if (nullptr != _buffer) {
                _buffer->commit(_handle);
            }
        }

        void* allocate(uint32_t size, bool const no_wait = false) {
            void* ptr = nullptr;
            if (uint32_t(-1) == _handle && nullptr != _buffer) {
                ptr = _buffer->allocate(size, _handle, no_wait);
            }
            return ptr;
        }
    };

}
