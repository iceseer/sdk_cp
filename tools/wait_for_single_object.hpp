namespace threading {

    class wait_for_single_object {
        wait_for_single_object(wait_for_single_object const&) = delete;
        wait_for_single_object& operator=(wait_for_single_object const&) = delete;

        enum { wait_release = 0, wait_set = 1 };

        std::condition_variable _wait_cv;
        std::mutex              _wait_m;
        std::atomic_flag        _wait_lock;

    public:
        wait_for_single_object() {
            _wait_lock.test_and_set();
        }

        bool wait(timestamp const wait_timeout_ms) {
            std::unique_lock<std::mutex> _lock(_wait_m);
            return _wait_cv.wait_for(_lock, std::chrono::milliseconds(wait_timeout_ms), [&]() {
                return !_wait_lock.test_and_set();
            });
        }

        void set() {
            _wait_lock.clear();
            _wait_cv.notify_one();
        }
    };

}
