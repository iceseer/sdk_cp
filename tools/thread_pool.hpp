namespace threading {

template<typename __callback_type, size_t __pool_size, typename __alloc = std::allocator<uint8_t>> class thread_pool final {
public:
    using allocator_type = __alloc;

    using cb_func        = std::function<__callback_type>;
    using cb_list        = std::vector<cb_func, allocator_type>;
    using cb_list_ref    = cb_list&;
    using cb_list_ptr    = cb_list*;

    class task_type;
    using task_id        = size_t;
    using task_func      = std::function<void(task_type&)>;

    class task_type {
        task_id             id;
        task_func           func;
        std::atomic_flag    ready_to_run;

        spin_blocker _cb_cs;
        cb_list      _cbs;
        bool         _blocked;

        void run() {
            ready_to_run.clear();
        }

        friend class thread_pool;

    public:
        task_type(task_type const&)             = delete;
        task_type& operator=(task_type const&)  = delete;
        task_type& operator=(task_type&&)       = delete;

        task_type(task_type&& c) : id(c.id), func(std::forward<task_func>(c.func)), _blocked(c._blocked) {
            ready_to_run = c.ready_to_run;
        }
        task_type() : id(0ull), _blocked(false) {
            ready_to_run.test_and_set();
        }
        task_type(task_func&& f) : func(std::forward<task_func>(f)), _blocked(false) {
            static std::atomic<task_id> task_id_global(0ull);
            id = ++task_id_global;
            ready_to_run.test_and_set();
        }

        bool swap_and_block(cb_list_ref cbs) {
            tools::threading::spin_blocker_infinite __lock(_cb_cs);
            if (!_blocked) {
                cbs.swap(_cbs);
                _blocked = true;
                return true;
            }
            return false;
        }
        void clear() {
            try {
                func = nullptr;

                tools::threading::spin_blocker_infinite __lock(_cb_cs);
                _cbs.clear();
            }
            catch (...) {}
        }
        bool add_cb(cb_func&& f) noexcept(true) {
            if (!!f) {
                tools::threading::spin_blocker_infinite __lock(_cb_cs);
                try {
                    if (!_blocked) {
                        _cbs.emplace_back(std::forward<cb_func>(f));
                        return true;
                    }
                }
                catch (...) {}
            }
            return false;
        }
        task_id get_id() const { 
            return id; 
        }
        void swap(task_type& c) {
            tools::threading::spin_blocker_infinite __lock(_cb_cs);

            register task_id const tmp = id;
            id = c.id; c.id = tmp;

            func.swap(c.func);
            
            if (c.ready_to_run.test_and_set()) {
                ready_to_run.test_and_set();
            } else {
                ready_to_run.clear();
            }

            _cbs.swap(c._cbs);
            register bool const t_block = _blocked;
            _blocked = c._blocked;
            c._blocked = t_block;
        }
    };

private:
    using thread_ptr        = std::unique_ptr<std::thread, std::function<void(std::thread*)>>;

    enum struct e_worker_state {
        k_ws_initing = 0,
        k_ws_working = 1,
        k_ws_exit    = 2,
    };
    
    struct {
        std::atomic<e_worker_state> _state;
    }               _thread_context[__pool_size];
    thread_ptr      _threads[__pool_size];
    allocator_type  _alloc;

    mutable spin_blocker    _tasks_cs;
    std::list<task_type>    _tasks;
    wait_for_single_object  _have_task;
    size_t                  _task_id;

public:
    thread_pool(thread_pool const&)             = delete;
    thread_pool& operator=(thread_pool const&)  = delete;
    thread_pool& operator=(thread_pool&&)       = delete;

    thread_pool() : _task_id(0ull) {
    }
    thread_pool(allocator_type& alloc) : _alloc(alloc), _task_id(0ull) {
    }
    thread_pool(thread_pool&& c) {
        tools::threading::spin_blocker_infinite __lock(_tasks_cs);
        _tasks      = std::move(c._tasks);
        _task_id    = c._task_id;
        _alloc      = c._alloc;

        for (size_t ix = 0; ix < __pool_size; ++ix) {
            this->_thread_context[ix]._state = c._thread_context[ix]._state;
            this->_threads[ix] = std::move(c._threads[ix]);
        }

        if (!_tasks.empty()) {
            _have_task.set();
        }
    }
    ~thread_pool() {
        for (auto& t : _threads) {
            SDK_CP_ASSERT(!t);
        }
    }

    static void _thread_worker(thread_pool& __this, size_t const context_ix) {
        auto& context = __this._thread_context[context_ix];

        e_worker_state expect_init = e_worker_state::k_ws_initing;
        while(context._state.compare_exchange_weak(expect_init, e_worker_state::k_ws_initing, std::memory_order_relaxed));

        task_type active_task;
        do {
            __this._have_task.wait(1ull * 1000ull);
            while (__this.pop_task(active_task)) {
                SDK_CP_ASSERT(!!active_task.func);
                try {
                    active_task.func(active_task);
                } catch (std::exception& e) {
                    tools::log::error("Task #", active_task.id, " throwed an exception: ", e.what());
                } catch(...) {
                    tools::log::error("Task #", active_task.id, " throwed an exception.");
                }
                active_task.clear();
            }
        } while(e_worker_state::k_ws_working == context._state.load(std::memory_order_relaxed));
    }

    //      Добавление таска                 Агрегатор
    //      |                                    |
    //      V                                    V
    //      Проверяем, что нет числа             Проставляем число
    //      |                                    |
    //      V                                    V
    //      Проверяем таски                      Удаляем таски из отработанных
    //                                           |
    //                                           V
    //                                           Вызов колбэков

public:
    void init() {
        for (size_t ix = 0; ix < __pool_size; ++ix) {
            if (!_threads[ix]) {
                _thread_context[ix]._state.store(e_worker_state::k_ws_initing);
                _threads[ix] = std::move(memory::sdk_cp_make_unique<std::thread, allocator_type>(_alloc, _thread_worker, std::ref(*this), ix));
                _thread_context[ix]._state.store(e_worker_state::k_ws_working);
            }
        }
    }
    void deinit() {
        for (size_t ix = 0; ix < __pool_size; ++ix) {
            _thread_context[ix]._state = e_worker_state::k_ws_exit;
            _have_task.set();
        }

        for (auto& t : _threads) {
            if (!!t) {
                t->join();
                t.reset();
            }
        }
    }

public:
    bool add_task(task_type*& task, task_func&& f) noexcept(true) {
        if (!!f) {
            tools::threading::spin_blocker_infinite __lock(_tasks_cs);
            try {
                _tasks.emplace_back(std::forward<task_func>(f));
                task = &_tasks.back();
                return true;
            } catch(...) { }
        }
        return false;
    }

    void run_task(task_type& f) {
        f.run();
        _have_task.set();
    }

    bool pop_task(task_type& f) noexcept(true) {
        tools::threading::spin_blocker_infinite __lock(_tasks_cs);
        try {
            auto ptr = _tasks.begin();
            auto end = _tasks.end();

            while (ptr != end) {
                if (!ptr->ready_to_run.test_and_set()) {
                    ptr->swap(f);
                    _tasks.erase(ptr);
                    return true;
                }
                ++ptr;
            }
        }
        catch (...) {}
        return false;
    }

    size_t size() const {
        tools::threading::spin_blocker_infinite __lock(_tasks_cs);
        return _tasks.size();
    }
};

}