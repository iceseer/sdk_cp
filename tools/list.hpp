namespace list {

    template<typename __type> class Node final {
        template<typename T, typename F, typename...A>  friend T*   create(F&& alloc, A&&...args);
        template<typename T, typename F>                friend void destroy(F&&, T* const ptr);
        template<typename T, typename F>                friend void foreach(T* const ptr, F const& func);
        template<typename T, typename F>                friend void destroy_all(F&&, T*& ptr);
        template<typename T>                            friend void insert_after (T* const after, T* const target);
        template<typename T>                            friend void remove       (T* const ptr);
        template<typename T>                            friend T*   next(T* ptr);

        __type _data;
        Node*  _prev;
        Node*  _next;

    public:
        Node(Node const&) = delete;
        Node& operator=(Node const&) = delete;

        template<typename...__args> explicit Node(__args&&...args) : _data(std::forward<__args>(args)...), _prev(this), _next(this) {

        }
    };

    template<typename __type, typename __func, typename...__args> SDK_CP_INLINE __type* create(__func&& alloc, __args&&...args) {
        if (auto ptr = alloc(sizeof(Node<__type>))) {
            return &(new(ptr) Node<__type>(std::forward<__args>(args)...))->_data;
        }
        return nullptr;
    }

    template<typename __type> SDK_CP_INLINE void insert_after(__type* const after, __type* const target) {
        SDK_CP_ASSERT(nullptr != after);
        SDK_CP_ASSERT(nullptr != target);

        auto const l = reinterpret_cast<Node<__type>*>(after);
        auto const r = l->_next;
        auto const t = reinterpret_cast<Node<__type>*>(target);

        l->_next = t;
        t->_prev = l;
        t->_next = r;
        r->_prev = t;
    }

    template<typename __type> SDK_CP_INLINE void remove(__type* const ptr) {
        SDK_CP_ASSERT(nullptr != ptr);

        auto const t = reinterpret_cast<Node<__type>*>(ptr);
        auto const l = t->_prev;
        auto const r = t->_next;

        l->_next = r;
        r->_prev = l;
    }

    template<typename __type, typename __func> SDK_CP_INLINE void destroy(__func&& dealloc, __type* const ptr) {
        SDK_CP_ASSERT(nullptr != ptr);

        auto const t = reinterpret_cast<Node<__type>*>(ptr);
        auto const l = t->_prev;
        auto const r = t->_next;

        l->_next = r;
        r->_prev = l;

        t->~Node<__type>();
        dealloc(t);
    }

    template<typename __type, typename __func> SDK_CP_INLINE void destroy_all(__func&& dealloc, __type*& ptr) {
        SDK_CP_ASSERT(nullptr != ptr);

        auto const t = reinterpret_cast<Node<__type>*>(ptr);
        auto n = t->_next;

        while(t != n) {
            n = n->_next;

            n->_prev->~Node<__type>();
            dealloc(n->_prev);
        }
        t->~Node<__type>();
        dealloc(t);
        ptr = nullptr;
    }

    template<typename __type> SDK_CP_INLINE __type* next(__type* ptr) {
        SDK_CP_ASSERT(nullptr != ptr);

        auto const t = reinterpret_cast<Node<__type>*>(ptr);
        SDK_CP_ASSERT(nullptr != t->_next);

        return &t->_next->_data;
    }

    template<typename __type, typename __func> SDK_CP_INLINE void foreach(__type* const ptr, __func const& func) {
        SDK_CP_ASSERT(nullptr != ptr);

        auto const t = reinterpret_cast<Node<__type>*>(ptr);
        auto n = t;

        do {
            if (!func(n->_data)) { break; }
            n = n->_next;
        } while (t != n);
    }
}
