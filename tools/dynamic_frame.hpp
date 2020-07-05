///////////////////////////
// author: Alexander Lednev
// date: 14.02.2018
///////////////////////////
namespace containers {

    template<typename T> class dynamic_frame final {
        static_assert(std::is_pod<T>::value, "T must be POD");

        dynamic_frame(dynamic_frame const&) = delete;
        dynamic_frame& operator=(dynamic_frame const&) = delete;

        std::vector<T> _data;

        uint64_t _virtual_blocks;
        uint64_t _iterator;
        uint64_t _size;
        uint64_t _sz;

        T& _iterator_to_node(uint64_t const it) {
            return _data[it % _virtual_blocks];
        }

    public:
        dynamic_frame(uint64_t const initial_capacity) : _virtual_blocks(0), _iterator(0), _size(0), _sz(0) {
            _sz = initial_capacity;
            _virtual_blocks = _sz + 1;
            _data.resize(initial_capacity);
        }

        void reserve(uint64_t const sz) {
            if (sz > _sz) {
                std::vector<T> tmp;
                tmp.reserve(sz + 1);

                for (int64_t ix = _size - 1; ix >= 0; --ix) {
                    tmp.push_back(get_from_first(ix));
                }

                tmp.resize(sz + 1);
                tmp.swap(_data);

                _sz = sz;
                _virtual_blocks = _sz + 1;
                _iterator = _size - 1;
            }
        }
        uint64_t size() const {
            return _size;
        }
        uint64_t capacity() const {
            return _sz;
        }

        T& get_from_first(uint32_t offset) {
            SDK_CP_ASSERT(offset < _sz);
            SDK_CP_ASSERT(offset >= 0);
            return _iterator_to_node(_iterator + _virtual_blocks - offset);
        }
        T& get_first() {
            return _iterator_to_node(_iterator);
        }
        T& get_last() {
            return _iterator_to_node(_iterator + _virtual_blocks - _sz + 1);
        }

        T& move_fwd(std::function<void(T&)>&& f) {
            f(get_last());

            ++_iterator;
            _iterator %= _virtual_blocks;

            ++_size;
            _size = sdk_cp_min(_size, _sz);
            return get_first();
        }
        void move_bck(std::function<void(T&)>&& f) {
            f(get_first());
            _iterator = (_iterator + _virtual_blocks - 1) % _virtual_blocks;

            --_size;
            _size = sdk_cp_min(_size, (uint64_t)0);
        }
    };

}
