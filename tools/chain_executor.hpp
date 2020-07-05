///////////////////////////
// author: Alexander Lednev
// date: 27.08.2018
///////////////////////////

namespace threading {

    template<
        typename T/*,
        typename A = std::allocator<T>*/
    > struct task_chain final {
        using task_type = T;
        //using allocator_type = A;

        task_chain(task_chain&&)                   = delete;
        task_chain(task_chain const&)              = delete;
        task_chain& operator = (task_chain const&) = delete;
        task_chain& operator = (task_chain&&)      = delete;

        task_chain() : _is_pending(false)
        { }
        //task_chain(allocator_type const& a)
        //    : _tasks(a)
        //    , _is_pending(false)
        //{ }
        ~task_chain(void) = default;

        inline bool add_task(task_type&& value) {
            threading::spin_blocker_infinite guard(_tasks_cs);
            _tasks.emplace_back(std::forward<task_type>(value));

            if (!_is_pending) {
                _is_pending = true;
                return true;
            }
            return false;
        }

        bool get_task(task_type& value) {
            threading::spin_blocker_infinite guard(_tasks_cs);
            if (_tasks.empty()) {
                _is_pending  = false;
            } else {
                SDK_CP_ASSERT(_is_pending);
                _tasks.front().swap(value);
                _tasks.pop_front();
            }
            return _is_pending;
        }

    private:
        using task_list_type = std::deque<task_type/*, allocator_type*/>;

        task_list_type          _tasks;
        threading::spin_blocker _tasks_cs;
        bool                    _is_pending;
    };

    template<
        typename K,
        typename T,
        typename H = std::hash<K>,
        typename KE = std::equal_to<K>/*,
        typename A = std::allocator<T>*/
    > struct task_syncronizer final {
        using task_type = T;
        //using allocator_type = A;
        using key_type = K;
        using hasher_type = H;
        using eq_type = KE;

    private:
        using task_chain_type = task_chain<task_type/*, allocator_type*/>;
        using chain_ptr       = std::shared_ptr<task_chain_type>;
        using chain_wptr      = std::weak_ptr<task_chain_type>;

        using key_tasks_list = std::unordered_map<key_type, chain_ptr, hasher_type, eq_type/*, allocator_type*/>;
        using pending_list   = std::deque<chain_wptr/*, allocator_type*/>;

        key_tasks_list          _chains_list;
        pending_list            _ready2work_list;
        threading::spin_blocker _lists_cs;

    public:
        task_syncronizer()
        { }
        //task_syncronizer(allocator_type const& a) : _chains_list(a), _ready2work_list(a)
        //{ }

        void add_task(key_type const& key, task_type&& value) {
            chain_ptr chain;
            {
                threading::spin_blocker_infinite lock(_lists_cs);
                auto& _ = _chains_list[key];
                if (!_) {
                    _ = std::make_shared<task_chain_type>(/*_ready2work_list.get_allocator()*/);
                }
                chain = _;
            }

            SDK_CP_ASSERT(!!chain);
            if (chain->add_task(std::move(value))) {
                threading::spin_blocker_infinite lock(_lists_cs);
                _ready2work_list.emplace_back(chain);
            }
        }

        auto pop_chain() {
            chain_ptr chain;
            threading::spin_blocker_infinite lock(_lists_cs);
            while (!_ready2work_list.empty()) {
                chain = _ready2work_list.front().lock();
                _ready2work_list.pop_front();
                if (!!chain) break;
            }
            return std::move(chain);
        }

        void remove_chain(key_type const& key) {
            threading::spin_blocker_infinite lock(_lists_cs);
            _chains_list.erase(key);
        }
    };

    struct Job {
    public:
        virtual ~Job() { }
        virtual void execute() = 0;
    };
    typedef std::shared_ptr<Job> JobPtr;

    template <typename RetType> struct AnyJob final : Job {
        AnyJob(std::packaged_task<RetType()>&& task)
            : task_(std::move(task)) {
                SDK_CP_ASSERT(!!task_.valid());
        }

        void execute() {
            SDK_CP_ASSERT(!!task_.valid());
            task_();
        }

        std::future<RetType> future() {
            SDK_CP_ASSERT(task_.valid());
            return task_.get_future();
        }

    private:
        std::packaged_task<RetType()> task_;
    };

    class chain_pool final {
    public:
        chain_pool(size_t num_threads) : _stop(false) {
            num_threads = (0 == num_threads) ?
                std::thread::hardware_concurrency() :
                num_threads;

            // num_threads = std::min(size_t(12), std::max(size_t(1), num_threads));
            // assert(num_threads >= 1 && num_threads <= 12);
            SDK_CP_ASSERT(num_threads >= 1);

            _threads.reserve(num_threads);
            for (size_t i = 0; i < num_threads; ++i) {
                _threads.emplace_back(std::thread(&chain_pool::threadManager, this));
            }

        }
        ~chain_pool() {
            _stop = true;
            for (auto& th : _threads) {
                th.join();
            }
        }

        chain_pool() = delete;
        chain_pool(chain_pool const&) = delete;
        chain_pool& operator= (chain_pool const&) = delete;
        chain_pool(chain_pool&&) = delete;
        chain_pool& operator= (chain_pool&&) = delete;

        template <typename Func, typename... Args >
        inline std::future<typename std::result_of<Func(Args...)>::type> add_job(uint64_t key, Func&& f, Args&&... args) {
            using return_type = typename std::result_of<Func(Args...)>::type;
            auto task = std::make_shared<AnyJob<return_type>>(
                                    std::packaged_task<return_type()>(std::bind(std::forward<Func>(f), std::forward<Args>(args)...)));

            auto future = task->future();
            _task_syncro.add_task(key, std::move(task));
            _have_tasks.set();
            return std::move(future);
        }

    private:
        inline void threadManager() {
            using namespace std::chrono_literals;

            JobPtr job;
            while (!_stop) {
                _have_tasks.wait(1000);
                if (_stop) return;

                if (auto chain = _task_syncro.pop_chain()) {
                    SDK_CP_ASSERT(!!chain);
                    while (!_stop && chain->get_task(job)) {
                        SDK_CP_ASSERT(!!job);
                        job->execute();
                    }
                }
            }
        }

    protected:
        bool                                _stop;
        std::vector<std::thread>            _threads;

        threading::wait_for_single_object   _have_tasks;
        task_syncronizer<uint64_t, JobPtr>  _task_syncro;
    };
}
