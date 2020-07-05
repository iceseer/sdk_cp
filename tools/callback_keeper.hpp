template<typename __function_type> class callback_unit final {
public:
    using callback_type = std::function<__function_type>;

private:
    tools::threading::spin_blocker  _callback_cs;
    callback_type                   _callback;

public:
    callback_unit()  = default;
    ~callback_unit() = default;

    callback_unit(callback_unit const&)             = delete;
    callback_unit& operator=(callback_unit const&)  = delete;
    callback_unit& operator=(callback_unit&&)       = delete;

public:
    template<typename __func>
    SDK_CP_INLINE void bind(__func&& f) {
        tools::threading::spin_blocker_infinite __guard(_callback_cs);
        _callback = std::forward<__func>(f);
    }

    template<typename...__args> SDK_CP_INLINE std::result_of_t<callback_type(__args...)> operator()(__args&&...args) {
        tools::threading::spin_blocker_infinite __guard(_callback_cs);
        SDK_CP_ASSERT(_callback != nullptr);
        return _callback(args...);
    }

    SDK_CP_INLINE bool operator!() {
        tools::threading::spin_blocker_infinite __guard(_callback_cs);
        return (_callback == nullptr);
    }
};

template<typename S, typename K, typename T> class callback_keeper final {
public:
    using value_type   = T;
    using value_ptr    = T*;

    using session_type = S;
    using session_ptr  = S*;

    using key_type      = K;
    using key_const_ref = K const&;
    typedef std::function<void(session_ptr, value_ptr)> callback_type;

private:
    tools::threading::spin_blocker              _callbacks_cs;
    std::unordered_map<key_type, callback_type> _callbacks;

public:
    callback_keeper() = default;
    callback_keeper(callback_keeper const&)             = delete;
    callback_keeper& operator=(callback_keeper const&)  = delete;
    callback_keeper& operator=(callback_keeper&&)       = delete;

    ~callback_keeper() {
        tools::threading::spin_blocker_infinite __guard(_callbacks_cs);
        for(auto it = _callbacks.begin(); it != _callbacks.end(); ++it) {
            it->second(nullptr, nullptr);
        }
    }

public:
    void bind(key_const_ref key, callback_type&& f) {
        callback_type callback;
        {
            tools::threading::spin_blocker_infinite __guard(_callbacks_cs);
            auto it = _callbacks.find(key);
            if (_callbacks.end() != it) {
                callback.swap(it->second);
            }
            _callbacks[key] = std::forward<callback_type>(f);
        }

        if (!!callback) {
            callback(nullptr, nullptr);
        }
    }

    void process(session_ptr session, key_const_ref key, value_ptr data) {
        callback_type callback;
        {
            tools::threading::spin_blocker_infinite __guard(_callbacks_cs);
            auto it = _callbacks.find(key);
            if (_callbacks.end() != it) {
                callback.swap(it->second);
                _callbacks.erase(it);
            }
            else {
                tools::log::warning("Callback handler was not found for request_id = ", key, ".");
            }
        }

        if (!!callback) {
            callback(session, data);
        }
    }
};

