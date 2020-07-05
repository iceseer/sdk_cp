///////////////////////////
// author: Alexander Lednev
// date: 14.02.2018
///////////////////////////
namespace containers {

    template<typename T, uint64_t SZ> class static_frame final {
        static_assert(SZ > 0, "Invalid SZ value.");

        static_frame(static_frame const&) = delete;
        static_frame& operator=(static_frame const&) = delete;

        enum { k_virt_blocks = SZ + 1 };
        T _data[k_virt_blocks];

        uint64_t _iterator;
        uint64_t _size;

        T& _iterator_to_node(uint64_t const it) {
            return _data[it % k_virt_blocks];
        }

    public:
        static_frame() : _iterator(0), _size(0) {

        }

        uint64_t size() const {
            return _size;
        }
        uint64_t capacity() const {
            return SZ;
        }

        T& get_from_first(int offset) {
            SDK_CP_ASSERT(offset < SZ);
            SDK_CP_ASSERT(offset >= 0);
            return _iterator_to_node(_iterator + k_virt_blocks - offset);
        }
        T& get_first() {
            return _iterator_to_node(_iterator);
        }
        T& get_last() {
            return _iterator_to_node(_iterator + k_virt_blocks - SZ + 1);
        }

        template<typename __func> void pop_back(__func&& f) {
            if (_size > 0) {
                auto& t = get_from_first(_size - 1);
                f(t);
                --_size;
            }
        }
        template<typename __func> T& move_fwd(__func&& f) {
            f(get_last());

            ++_iterator;
            _iterator %= k_virt_blocks;

            ++_size;
            _size = sdk_cp_min(_size, (uint64_t)SZ);
            return get_first();
        }
        template<typename __func> void move_bck(__func&& f) {
            f(get_first());
            _iterator = (_iterator + k_virt_blocks - 1) % k_virt_blocks;

            --_size;
            _size = sdk_cp_min(_size, (uint64_t)0);
        }
    };

}
