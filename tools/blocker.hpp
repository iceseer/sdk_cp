namespace threading {

    class spin_blocker final {
        std::atomic_flag _blocker;

    public:
#   ifdef __GNUG__
        spin_blocker() : _blocker(ATOMIC_FLAG_INIT) {
        }
#   elif _WIN32
        spin_blocker() {
            _blocker.clear();
        }
#   endif

        void lock() {
            while (_blocker.test_and_set(std::memory_order_acquire));
        }
        bool tryLock() {
            return !_blocker.test_and_set(std::memory_order_acquire);
        }
        void unlock() {
            _blocker.clear(std::memory_order_release);
        }
    };

    class spin_blocker_infinite final
    {
        spin_blocker_infinite(const spin_blocker_infinite &)  = delete;
        spin_blocker_infinite(const spin_blocker_infinite &&) = delete;

        spin_blocker_infinite & operator=(const spin_blocker_infinite &)  = delete;
        spin_blocker_infinite & operator=(const spin_blocker_infinite &&) = delete;

        spin_blocker& _blocker;

    public:
        spin_blocker_infinite(spin_blocker& blocker) : _blocker(blocker) {
            _blocker.lock();
        }
        ~spin_blocker_infinite() {
            _blocker.unlock();
        }
    };

    class spin_blocker_try final
    {
        spin_blocker_try(const spin_blocker_try &)  = delete;
        spin_blocker_try(const spin_blocker_try &&) = delete;

        spin_blocker_try & operator=(const spin_blocker_try &)  = delete;
        spin_blocker_try & operator=(const spin_blocker_try &&) = delete;

        spin_blocker& _blocker;
        bool          _meLocked;

    public:
        spin_blocker_try(spin_blocker& blocker) : _blocker(blocker) {
            _meLocked = _blocker.tryLock();
        }
        ~spin_blocker_try() {
            if (_meLocked) {
                _blocker.unlock();
            }
        }
        bool meLocked() {
            return _meLocked;
        }
    };

}
