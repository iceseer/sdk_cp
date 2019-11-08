///////////////////////////
// author: Alexander Lednev
// date: 27.08.2018
///////////////////////////

namespace allocators {
    template<uint64_t __chunk_size> class heap {
        typedef std::unique_ptr<heap_allocator<__chunk_size>> heap_chunk_pointer;
        typedef struct {
            uintptr_t pointer_begin;
            uintptr_t pointer_end;
            heap_chunk_pointer chunk;
        } heap_chunk_info;
        typedef std::vector<heap_chunk_info> heap_chunk_array;

        threading::spin_blocker _heap_chunks_guard;
        heap_chunk_array        _heap_chunks;

    public:
        heap(heap const&) = delete;
        heap& operator=(heap const&) = delete;

        heap() = default;
        ~heap() = default;

        uint8_t* allocate(uint64_t const size) {
            if (2 * size <= __chunk_size) {
                tools::threading::spin_blocker_infinite __chunk_list_lock(_heap_chunks_guard);
                
                for (uint64_t ix = 0; ix < _heap_chunks.size(); ++ix) {
                    auto& it = _heap_chunks[ix];
                    if (auto* ptr = it.chunk->allocate(size)) {
                        return (uint8_t*)ptr;
                    }
                }

                heap_chunk_pointer chunk(new(std::nothrow) heap_allocator<__chunk_size>());
                if (!!chunk) {
                    _heap_chunks.push_back({ chunk->get_memory_begin(), chunk->get_memory_end(), std::move(chunk) });
                    auto& it = _heap_chunks[_heap_chunks.size() - 1];
                    if (auto* ptr = it.chunk->allocate(size)) {
                        return (uint8_t*)ptr;
                    }
                }
            }
            return (uint8_t*)malloc(size);
        }

        bool deallocate(uint8_t* const p) {
            tools::threading::spin_blocker_infinite __chunk_list_lock(_heap_chunks_guard);
            for (auto& it : _heap_chunks) {
                if ((uintptr_t)p >= it.pointer_begin && (uintptr_t)p < it.pointer_end) {
                    return it.chunk->release(p);
                }
            }
            free(p);
            return true;
        }
    };

    template<typename T, uint64_t __chunk_size> class std_chunk_hallocator {
        heap<__chunk_size>& _heap;

    public:
        typedef T  value_type;
        typedef T* pointer;
        typedef T& reference;
        typedef T const& const_reference;
        typedef T const* const_pointer;
        typedef size_t size_type;

        template<typename U> struct rebind {
            typedef std_chunk_hallocator<U, __chunk_size> other;
        };

    public:
        std_chunk_hallocator(heap<__chunk_size>& heap) : _heap(heap) { }
        ~std_chunk_hallocator() = default;

        pointer allocate (size_type n, const_pointer hint=0) {
            if (auto* ptr = _heap.allocate(n * sizeof(value_type))) {
                return (pointer)ptr;
            }
            return nullptr;
        }

        void deallocate(pointer p, size_type n) {
            _heap.deallocate((uint8_t*)p);
        }
    };
}