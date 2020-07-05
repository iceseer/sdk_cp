template<typename __request_handler, uint32_t __num_threads> class server_ws final : public std::enable_shared_from_this<server_ws<__request_handler, __num_threads>> {
    struct request_argument {
        server_ws* __this;
        uint64_t   ix;
    };

    tools::threading::spin_blocker pending_list_cs;
    std::unordered_map<uintptr_t, std::shared_ptr<user_t>> pending_list;

    bool const _encrypt;

    void _listencb(struct event_base* eb, struct bufferevent* bev, struct sockaddr* /*addr*/, int /*len*/, uint64_t worker_ix, evutil_socket_t clisockfd) {
        auto user = std::make_shared<user_t>(clisockfd, _encrypt);
        std::weak_ptr<server_ws> __this = shared_from_this();

        user->on_disconnect.bind([__this](user_t* u) {
            auto server = __this.lock();
            if (!!server) {
                tools::threading::spin_blocker_infinite __guard(server->pending_list_cs);
                server->pending_list.erase((uintptr_t)u);
            }
        });

        user->on_frame_recv.bind([__this, worker_ix](user_t* u) {
            auto server = __this.lock();
            SDK_CP_ASSERT(!!server);
            if (!server) { return; }

            auto& wc = server->_worker_context[worker_ix];
            u->extract_data(wc.direct_data);

            std::shared_ptr<user_t> ws_user_ptr;
            {
                tools::threading::spin_blocker_infinite __guard(server->pending_list_cs);
                auto it = server->pending_list.find((uintptr_t)u);
                SDK_CP_ASSERT(it != server->pending_list.end());

                ws_user_ptr = it->second;
            }

            server_call_context<server_ws<__request_handler, __num_threads>> call_context(
                wc.request, 
                wc.response, 
                nullptr, 
                server, 
                worker_ix, 
                wc.direct_data, 
                wc.stack
            );
            call_context.user_connection = ws_user_ptr;

            try {
                Json::Reader reader;
                if (!reader.parse(wc.direct_data, call_context.request)) {
                    tools::log::error("Parse json failed. Incorrect data: ", wc.direct_data.c_str());

                    SDK_CP_ASSERT(!!ws_user_ptr);
                    ws_user_ptr->disconnect();
                    return;
                }

                call_context.response.clear();
                call_context.direct_data.clear();
                wc.stack.clear();

                user_t::fb_begin_prepare(call_context.direct_data);
                if (!wc.rh->on_request(call_context)) {
                    //tools::log::error("Process request failed. 'method' call invalid.");

                    //SDK_CP_ASSERT(!!ws_user_ptr);
                    //ws_user_ptr->disconnect();
                    return;
                }

                if (call_context.direct_data.size() > frame_buffer_t::k_prefix_size) {
                    //auto fb = user_t::fb_end_prepare(u->is_encrypt(), u->get_aes_key(), u->get_aes_ivc(), call_context.direct_data, wc.response_str);
                    //u->send(fb);
                }
                else if (!call_context.response.empty()) {
                    //wc.response_str.clear();
                    //wc.response_str.reserve(1024);
                    //user_t::fb_begin_prepare(wc.response_str);

                    //tools::json_writer<std::string> writer;
                    //writer.write(call_context.response, wc.response_str);
                    //auto fb = user_t::fb_end_prepare(u->is_encrypt(), u->get_aes_key(), u->get_aes_ivc(), wc.response_str, call_context.direct_data);

                    //Json::FastWriter fastWriter;
                    //auto response_str = fastWriter.write(call_context.response);
                    //u->send(fb);
                }

                return;
            }
            catch (std::exception& e) {
                tools::log::error("Exception. Worker error. Description=", e.what());
            }
            catch (...) {
                tools::log::error("Exception. Worker error.");
            }

            SDK_CP_ASSERT(!!ws_user_ptr);
            ws_user_ptr->disconnect();
        });

        {
            tools::threading::spin_blocker_infinite __guard(pending_list_cs);
            pending_list.emplace(std::pair<uintptr_t, std::shared_ptr<user_t>>((uintptr_t)user.get(), user));
        }
        user->attach_buffer_eventsock(bev);
        user->start_handling();
    }

    static void _exit_ev_callback(evutil_socket_t sock, short what, void *arg) {
        struct event_base *base = (event_base*)arg;
        event_base_loopbreak(base);
    }

    static void listencb(struct evconnlistener *listener, evutil_socket_t clisockfd, struct sockaddr *addr, int len, void *arg) {
        SDK_CP_ASSERT(nullptr != arg);

        try {
            event_base* eb   = evconnlistener_get_base(listener);
            bufferevent* bev = bufferevent_socket_new(eb, clisockfd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);// TODO: проверить BEV_OPT_THREADSAFE
            LOG("[WS] A user logined in, socketfd = %d", bufferevent_getfd(bev));

            auto ra = (request_argument*)arg;
            ra->__this->_listencb(eb, bev, addr, len, ra->ix, clisockfd);
        } catch (std::exception& e) {
            tools::log::error("[WS] EXCEPTION: ", e.what());
        } catch (...) {
            tools::log::error("[WS] UNEXPECTED EXCEPTION");
        }
    }

    static void on_worker(server_ws* __this, uint64_t ix) {
        try {
#   ifdef __GNUG__
            tools::log::info("Worker thread \033[1;32mSTARTED\033[0m: ix = ", ix);
#   elif _WIN32
            tools::log::info("Worker thread STARTED: ix = ", ix);
#   endif
            __this->_worker_proc(ix);
        } catch(std::exception& e) {
            tools::log::error("Exception. Worker error. Description=", e.what());
        } catch(...) {
            tools::log::error("Exception. Worker error.");
        }

#   ifdef __GNUG__
        tools::log::info("Worker thread \033[1;32mSTOPED\033[0m: ix = ", ix);
#   elif _WIN32
        tools::log::info("Worker thread STOPED: ix = ", ix);
#   endif
    }
    void _worker_proc(uint64_t const ix) {
        if (lzo_init() != LZO_E_OK) {
            tools::log::error("LZO init failed.");
            return;
        }

        auto& wc = _worker_context[ix];
        wc.lzo_tmp_memory.resize(LZO1X_1_MEM_COMPRESS);
        wc.rh->bind_thread_id();

        WSADATA wsaData = { 0 };
        auto const result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            tools::log::error("WSAStartup failed: err=", result);
            return;
        }

        auto& fd = wc.fd;
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            tools::log::error("Create socket failed: err=", strerror(errno));
            return;
        }

        if (evutil_make_socket_nonblocking(fd) < 0) {
            tools::log::error("Set socket nonblocking failed: err=", strerror(errno));
            return;
        }

        struct sockaddr_in sin;
        sin.sin_family      = AF_INET;
        sin.sin_port        = htons((uint16_t)wc.port);
        sin.sin_addr.s_addr = inet_addr(wc.host.c_str());
        int on = 1;

#   ifdef __GNUG__
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
#   elif _WIN32
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) == -1) {
#   endif
            tools::log::error("Set socket option SO_REUSEADDR failed: err=", strerror(errno));
            return;
        }

        //struct sockaddr_in srvaddr;
        //srvaddr.sin_family = AF_INET;
        //srvaddr.sin_port = htons(wc.port);
        //srvaddr.sin_addr.s_addr = inet_addr(wc.host.c_str());

        request_argument ra;
        ra.__this = this;
        ra.ix = ix;

        if (::bind(fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
            tools::log::error("Bind socket failed: err=", strerror(errno));
            return;
        }

        auto cfg = _make_shared(event_config_new(), &event_config_free);
        if (!cfg) {
            tools::log::error("Unable to create libevent config: err=", strerror(errno));
            return;
        }
        event_config_avoid_method(cfg.get(), "select");

        auto base = _make_shared(event_base_new_with_config(cfg.get()), &event_base_free);
        if (!base) {
            tools::log::error("Event base creation failed: err=", strerror(errno));
            return;
        }
        cfg.reset();

        //auto base = _make_shared(event_base_new(), &event_base_free);
        //if (!base) {
        //    tools::log::error("Event base creation failed: err=", strerror(errno));
        //    return;
        //}

        auto listener = _make_shared(
            evconnlistener_new(base.get(), listencb, &ra, LEV_OPT_REUSEABLE_PORT | LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, 500, fd),
            &evconnlistener_free);
        if (!listener) {
            tools::log::error("Event listener creation failed: err=", strerror(errno));
            return;
        }

        //auto listener = _make_shared(
        //    evconnlistener_new_bind(base.get(), listencb, &ra, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, 500, (const struct sockaddr*)&srvaddr, sizeof(struct sockaddr)),
        //    &evconnlistener_free);
        //if (!listener) {
        //    tools::log::error("Event listener creation failed: err=", strerror(errno));
        //    return;
        //}

        wc.watchdog = _make_shared(event_new(base.get(), -1, EV_TIMEOUT | EV_READ, _exit_ev_callback, base.get()), &event_free);
        if (0 != event_add(wc.watchdog.get(), NULL)) {
            tools::log::error("Add exit event failed");
            return;
        }

        event_base_dispatch(base.get());

        wc.watchdog.reset();
        listener.reset();
        base.reset();

#   ifdef __GNUG__
        close(fd);
#   elif _WIN32
        closesocket(fd);
#   endif
    }

    template<typename T> inline std::shared_ptr<T> _make_shared(T* t, void(*f)(T*)) {
        return std::shared_ptr<T>(t, [f](T* obj) {
            if (nullptr != obj) {
                f(obj);
            }
        });
    }

    struct uploader_context {
        using function_type = std::function<bool(server_call_context<server_ws<__request_handler, __num_threads>>&)>;

        function_type f;
        void*         h;

        uploader_context(function_type&& ff, void* hh) : f(std::forward<function_type>(ff)), h(hh) {
        }
    };
    struct {
        std::shared_ptr<__request_handler> rh;
        addition_stack                     stack;

        std::vector<uint8_t>               request_data;
        std::vector<uint8_t>               uncompressed_data;
        std::vector<uint8_t>               lzo_tmp_memory;

        std::string                        response_str;

        Json::Value                        request;
        Json::Value                        response;
        std::string                        direct_data;
        std::unique_ptr<std::thread>       worker_thread;

        std::string                        host;
        uint64_t                           port;

        SOCKET                             fd;
        std::shared_ptr<struct event>      watchdog;
        std::shared_ptr<struct event>      event_upload;

        tools::threading::spin_blocker uploaders_cs;
        std::vector<uploader_context>  uploaders;
    } _worker_context[__num_threads];

public:
    server_ws(std::shared_ptr<__request_handler> (&rh)[__num_threads], bool const encrypt) : _encrypt(encrypt) {
        for (uint64_t ix = 0; ix < __num_threads; ++ix) {
            auto& wc = _worker_context[ix];
            wc.rh = rh[ix];
        }
    }
    ~server_ws() {
        stop();
    }

    void run(std::string const& host, uint64_t const port) {
        SDK_CP_ASSERT(!host.empty());
        SDK_CP_ASSERT(0 != port);
        evthread_use_windows_threads();

        tools::log::info("Start web socket server at: host=", host.c_str(), ", port=", port);
        for (auto& wc : _worker_context) {
            if (!!wc.worker_thread) {
                tools::log::error("Workers already inited. Stop first.");
                return;
            }
            if (!wc.rh) {
                tools::log::error("Some of request handlers not defined.");
                return;
            }
            wc.port = port;
            wc.host = host;
        }

        for (uint64_t ix = 0; ix < __num_threads; ++ix) {
            auto& wc = _worker_context[ix];
            wc.worker_thread.reset(new std::thread(&on_worker, this, ix));
        }
    }
    void register_uploader(server_call_context<server_ws<__request_handler, __num_threads>> const& cc, std::function<bool(server_call_context<server_ws<__request_handler, __num_threads>>&)>&& f) {
        auto& wc = _worker_context[cc.request_handler_ix];
        {
            tools::threading::spin_blocker_infinite guard(wc.uploaders_cs);
            wc.uploaders.emplace_back(std::forward<std::function<bool(server_call_context<server_ws<__request_handler, __num_threads>>&)>>(f), cc.handle);
        }
        event_active(wc.event_upload.get(), EV_UPLOAD, 0);
    }
    void stop() {
        for (auto& wc : _worker_context) {
           // closesocket(wc.fd);
            if (!!wc.watchdog) {
                event_active(wc.watchdog.get(), EV_READ | EV_WRITE, 1);
            }
        }

        //for (auto& wc : _worker_context) {
        //    closesocket(wc.fd);
        //    //if (!!wc.watchdog) {
        //    //    event_active(wc.watchdog.get(), EV_READ | EV_WRITE, 1);
        //    //}
        //}
        for (uint64_t ix = 0; ix < __num_threads; ++ix) {
            auto& wc = _worker_context[ix];
            if (!!wc.worker_thread) {
                wc.worker_thread->join();
                wc.worker_thread.reset();
            }
        }
    }
};
