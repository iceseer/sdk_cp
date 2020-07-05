namespace threading {

    template<typename T, uint64_t N = 1ull * 1024 * 1024, uint64_t __Timeout = k_timeout_infinite> class threaded_handler {
        threaded_handler(const threaded_handler&) = delete;
        threaded_handler& operator=(const threaded_handler&) = delete;

        typedef containers::ring_buffer<N>                  buffer_type;
        typedef containers::buffer_auto_commit<buffer_type> buffer_handle_type;
        typedef std::function<void(const T*)>               F;

        buffer_type                   _rb;
        std::function<void(const T*)> _f;
        std::unique_ptr<std::thread>  _s;

        wait_for_single_object _have_event;
        spin_blocker           _state_block;

    protected:
        uint64_t const k_exit;
        uint64_t const k_work;

        std::atomic<uint64_t> _work_status;

        virtual void on_init_thread()     {}
        virtual void on_deinit_thread()   {}
        virtual void on_loop_thread()     {}
        virtual void on_loop_end_thread() {}

    private:

        static void _worker(threaded_handler& __this) { 
            __this.proc(); 
        }
        void proc() {
            on_init_thread();

            auto ab_deleter = [&](T* obj) {
                if (nullptr != obj) {
                    obj->~T();
                    _rb.free();
                }
            };

            while (_work_status != k_exit) {
                _have_event.wait(__Timeout);
                on_loop_thread();

                uint32_t sz;
                while (T* arg = (T*)_rb.peek(sz)) {
                    std::unique_ptr<T, decltype(ab_deleter)> __ab(arg, ab_deleter);
                    try {
                        if (!!_f) {
                            _f(arg);
                        }
                    }
                    catch (...) { }
                }

                on_loop_end_thread();
            }
            on_deinit_thread();
        }

        template<typename __Type, typename...__Args> bool _push(std::function<void*(buffer_handle_type&,uint32_t)>&& alloc_func, const __Args&...args) {
            {
                spin_blocker_infinite __lock(_state_block);
                if (!_s) {
                    _work_status = k_work;
                    _s.reset(new std::thread(_worker, std::ref(*this)));
                }
            }

            bool event_do_set = false;
            {
                buffer_handle_type bac(_rb);
                void* ptr = alloc_func(bac, sizeof(__Type));
                if (nullptr != ptr) {
                    event_do_set = true;
                    new(ptr) __Type(args...);
                }
            }

            if (event_do_set) {
                _have_event.set();
            }

            return event_do_set;
        }

    public:
        threaded_handler(const F& f) : _f(f), k_exit(0), k_work(1) {
        }

        threaded_handler() : k_exit(0), k_work(1) {
        }

        virtual ~threaded_handler() {
            stop();
        }

        void stop() {
            _work_status = k_exit;
            _have_event.set();

            if (!!_s) {
                _s->join();
                _s.reset();
            }
        }

        void setCallbackFunc(std::function<void(const T*)> const& f) {
            spin_blocker_infinite __lock(_state_block);
            _f = f;
        }
        template<typename __Type, typename...__Args> void push(const __Args&...args) {
            _push<__Type, __Args...>([](buffer_handle_type& bac, uint32_t size) {
                return bac.allocate(size);
            }, args...);
        }
        template<typename __Type, typename...__Args> bool push_no_wait(const __Args&...args) {
            return _push<__Type, __Args...>([](buffer_handle_type& bac, uint32_t size) {
                return bac.allocate(size, true);
            }, args...);
        }
    };

}