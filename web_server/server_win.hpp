//struct i_server_call_context {
//    virtual ~i_server_call_context() { }
//    virtual void*        handle()   = 0;
//    virtual Json::Value& request()  = 0;
//    virtual Json::Value& response() = 0;
//    virtual void hold(bool h)       = 0;
//    virtual std::function<void()> make_uploader(std::function<bool(i_server_call_context&)>&& f) = 0;
//};

template<typename __server_type> struct server_call_context final {
    using stack_type = addition_stack;

public:
    void*                        handle;
    Json::Value&                 request;
    Json::Value&                 response;
    std::string&                 direct_data;
    uint64_t                     request_handler_ix;
    std::weak_ptr<__server_type> server;
    bool                         hold;
    stack_type&                  stack;
    std::weak_ptr<user_t>        user_connection;

    server_call_context(
        Json::Value& req, 
        Json::Value& res, 
        void* h, 
        std::shared_ptr<__server_type>& s, 
        uint64_t rh, 
        std::string& dd,
        addition_stack& sk
    ) : request(req), response(res), hold(false), handle(h), server(s), request_handler_ix(rh), direct_data(dd), stack(sk) {
    }

    //void* handle() override          { return _handle;   }
    //Json::Value& request() override  { return _request;  }
    //Json::Value& response() override { return _response; }
    //void hold(bool h) override       { _hold = h;        }

    //std::function<void()> make_uploader(std::function<bool(i_server_call_context&)>&& f) override {
        //auto s  = _server;
        //auto rh = _request_handler_ix;
        //auto h  = _handle;
        //return [s, rh, f, h]() {
        //    auto serv = s.lock();
        //    if (!serv) {
        //        tools::log::error("Unable to register uploader. Request handler: ", rh);
        //        return;
        //    }
        //    serv->register_uploader(h, rh, f);
        //};
    //    return [](){};
    //}
};

template<typename __request_handler, uint32_t __num_threads> class server final
    : public std::enable_shared_from_this<server<__request_handler, __num_threads>> {

    struct request_argument {
        server*   __this;
        uint64_t  ix;
    };

    static void on_request(struct evhttp_request* req, void* arg) {
        if (nullptr != arg && nullptr != req) {
            auto ra = (request_argument*)arg;
            ra->__this->_process_request(req, ra->ix);
        } else {
            tools::log::error("On request argument is null. Send error. Request=", (uintptr_t)req, ", arg=", (uintptr_t)arg);
            evhttp_send_error(req, HTTP_INTERNAL, "Internal error(no argument)");
        }
    }

    static struct bufferevent* bev_cb(struct event_base *base, void *arg)
    {
        struct bufferevent* r;
        SSL_CTX *ctx = (SSL_CTX *)arg;
        r = bufferevent_openssl_socket_new(base,
            -1,
            SSL_new(ctx),
            BUFFEREVENT_SSL_ACCEPTING,
            BEV_OPT_CLOSE_ON_FREE);
        return r;
    }

    static void event_handler(evutil_socket_t fd, short ev, void* arg) {
        if (nullptr != arg) {
            auto ra = (request_argument*)arg;
            ra->__this->_event_handler(ev, ra->ix);
        }
        else {
            tools::log::error("On agregate uploaders catch an error. 'arg' is null.");
        }
    }

    template<typename...__args> void _print_openssl_error(__args&&...args)  {
        char buffer[512];
        ERR_error_string(ERR_get_error(), buffer);

        tools::log::error(std::forward<__args>(args)..., "\n[OPENSSL ERROR]: ", buffer);
    }

    bool _compress_response(size_t const worker_ix, std::string const& in_data, uint8_t const*& data, size_t& size, struct evkeyvalq*& headers) {
        SDK_CP_ASSERT(worker_ix < __num_threads);

        auto& wc                = _worker_context[worker_ix];
        auto& data_compressed   = wc.request_data;

        evhttp_add_header(headers, "Server",                        k_server_name);
        evhttp_add_header(headers, "Connection",                    "close");
        evhttp_add_header(headers, "Access-Control-Allow-Origin",   "*");
        evhttp_add_header(headers, "Access-Control-Allow-Headers",  "origin, content-type, accept");
        evhttp_add_header(headers, "Content-Type",                  "application/json");

        if (!_compression) {
            data = (uint8_t*)in_data.c_str();
            size = in_data.size();
            return true;
        }

        data_compressed.resize(in_data.size() + in_data.size() / 16 + 64 + 3);
        lzo_uint compressed_data_sz = 0;

        auto const result = lzo1x_1_compress(
            (uint8_t const*)in_data.c_str(),
            in_data.size(),
            &data_compressed[0],
            &compressed_data_sz,
            &wc.lzo_tmp_memory[0]
        );

        if (result != LZO_E_OK) {
            tools::log::error("Compression failed [ ", result, " ]");
            return false;
        }

        data = &data_compressed[0];
        size = compressed_data_sz;

        evhttp_add_header(headers, "Accept-Compression", "lzo");
        return true;
    }

    void _event_handler(short ev, uint64_t worker_ix) {
        auto& wc = _worker_context[worker_ix];

        if (ev & EV_UPLOAD) {
            server_call_context<server<__request_handler, __num_threads>> call_context(wc.request, wc.response, nullptr, shared_from_this(), worker_ix, wc.direct_data, wc.stack);
            wc.request.clear();

            tools::threading::spin_blocker_infinite guard(wc.uploaders_cs);
            for (auto& f : wc.uploaders) {
                call_context.handle = f.h;
                struct evhttp_request* req = (struct evhttp_request*)call_context.handle;

                call_context.response.clear();
                call_context.direct_data.clear();
                wc.stack.clear();

                if (!f.f(call_context)) {
                    tools::log::error("Process request failed. Uploader not prepared.");
                    evhttp_send_error(req, HTTP_BADMETHOD, "Process request failed. Uploader not prepared.");
                    return;
                }

                auto buffer_deleter = [](evbuffer* obj) { if (nullptr != obj) evbuffer_free(obj); };
                std::unique_ptr<evbuffer, decltype(buffer_deleter)> evb(evbuffer_new(), buffer_deleter);

                uint8_t const*  out_data = nullptr;
                size_t          out_size = 0;

                if (!call_context.direct_data.empty()) {
                    if (!_compress_response(worker_ix, call_context.direct_data, out_data, out_size, req->output_headers)) {
                        evhttp_send_error(req, HTTP_INTERNAL, "Compression failed.");
                        return;
                    }
                }
                else if (!call_context.response.empty()) {
                    wc.response_str.clear();
                    wc.response_str.reserve(1024);

                    tools::json_writer<std::string> writer;
                    writer.write(call_context.response, wc.response_str);

                    if (!_compress_response(worker_ix, wc.response_str, out_data, out_size, req->output_headers)) {
                        evhttp_send_error(req, HTTP_INTERNAL, "Compression failed.");
                        return;
                    }
                }

                if (0 == out_size) {
                    evhttp_send_reply(req, HTTP_NOCONTENT, "OK", nullptr);
                    return;
                }

                evbuffer_add(evb.get(), out_data, out_size);
                evhttp_send_reply(req, HTTP_OK, "OK", evb.get());
            }
            wc.uploaders.clear();
        }
    }

    static void on_worker(server* __this, uint64_t ix) {
        try {
#   ifdef __GNUG__
            tools::log::trace("Worker thread \033[1;32mSTARTED\033[0m: ix = ", ix);
#   elif _WIN32
            tools::log::trace("Worker thread STARTED: ix = ", ix);
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

    static void _exit_ev_callback(evutil_socket_t sock, short what, void *arg) {
        struct event_base *base = (event_base*)arg;
        event_base_loopbreak(base);
    }

    void _worker_proc(uint64_t const ix) {
        if (lzo_init() != LZO_E_OK) {
            tools::log::error("LZO init failed.");
            return;
        }

        auto& wc = _worker_context[ix];
        wc.lzo_tmp_memory.resize(LZO1X_1_MEM_COMPRESS);
        wc.rh->bind_thread_id();

#ifdef _WIN32
        WSADATA wsaData = { 0 };
        auto const result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            tools::log::error("WSAStartup failed: err=", result);
            return;
        }
#endif
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

        if (::bind(fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
            tools::log::error("Bind socket failed: err=", strerror(errno));
            return;
        }

        if (listen(fd, k_listen_sockets_count) < 0) {
            tools::log::error("Listen socket failed: err=", strerror(errno));
            return;
        }

        auto cfg = _make_shared(event_config_new(), &event_config_free);
        if (!cfg) {
            tools::log::error("Unable to create libevent config: err=", strerror(errno));
            return;
        }
        //event_config_set_num_cpus_hint(cfg.get(), 8);
        //event_config_set_flag(cfg.get(), EVENT_BASE_FLAG_STARTUP_IOCP);
        event_config_avoid_method(cfg.get(), "select");

        auto base = _make_shared(event_base_new_with_config(cfg.get()), &event_base_free);
        if (!base) {
            tools::log::error("Event base creation failed: err=", strerror(errno));
            return;
        }
        cfg.reset();

        auto http_server = _make_shared(evhttp_new(base.get()), &evhttp_free);
        if (!http_server) {
            tools::log::error("Http-server creation failed: err=", strerror(errno));
            return;
        }

        evhttp_set_allowed_methods(http_server.get(),
            EVHTTP_REQ_GET |
            EVHTTP_REQ_POST |
            EVHTTP_REQ_HEAD |
            EVHTTP_REQ_PUT |
            EVHTTP_REQ_DELETE |
            EVHTTP_REQ_OPTIONS);

        std::shared_ptr<SSL_CTX> ctx;
        std::shared_ptr<EC_KEY>  ecdh;
        if (_https) {
            ctx = _make_shared(SSL_CTX_new(SSLv23_server_method()), &SSL_CTX_free);
            if (!ctx) {
                _print_openssl_error("OpenSSL context creation failed.");
                return;
            }

            SSL_CTX_set_options(ctx.get(),
                SSL_OP_SINGLE_DH_USE |
                SSL_OP_SINGLE_ECDH_USE |
                SSL_OP_NO_SSLv2);

            ecdh = _make_shared(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1), &EC_KEY_free);
            if (!ecdh) {
                _print_openssl_error("OpenSSL curve create failed.");
                return;
            }
            if (1 != SSL_CTX_set_tmp_ecdh(ctx.get(), ecdh.get())) {
                _print_openssl_error("OpenSSL set curve failed.");
                return;
            }

            SDK_CP_ASSERT(_certificate != nullptr);
            SDK_CP_ASSERT(_private_key != nullptr);

            tools::log::info("Loading OpenSSL data...");
            if (1 != SSL_CTX_use_certificate_chain_file(ctx.get(), _certificate)) {
                _print_openssl_error("Load 'certificate' failed.");
                return;
            }

            if (1 != SSL_CTX_use_PrivateKey_file(ctx.get(), _private_key, SSL_FILETYPE_PEM)) {
                _print_openssl_error("Load 'private key' failed.");
                return;
            }

            if (1 != SSL_CTX_check_private_key(ctx.get())) {
                _print_openssl_error("Check 'private key' failed.");
                return;
            }
            tools::log::info("OpenSSL data loaded [OK].");

            evhttp_set_bevcb(http_server.get(), bev_cb, ctx.get());
        }

        request_argument ra;
        ra.__this = this;
        ra.ix     = ix;

        struct timeval tv;
        tv.tv_sec  = 100000;
        tv.tv_usec = 1000;

        wc.event_upload = _make_shared(event_new(base.get(), -1, 0, event_handler, &ra), &event_free);
        event_add(wc.event_upload.get(), nullptr);

#   ifdef __GNUG__
        signal(SIGPIPE, SIG_IGN);
#   endif//__GNUG__

        if (evhttp_accept_socket(http_server.get(), fd) == -1) {
            tools::log::error("Http accept socket failed: err=", strerror(errno));
            return;
        }

        evhttp_set_gencb(http_server.get(), on_request, &ra);

        wc.watchdog = _make_shared(event_new(base.get(), -1, EV_TIMEOUT | EV_READ, _exit_ev_callback, base.get()), &event_free);
        if (0 != event_add(wc.watchdog.get(), NULL)) {
            tools::log::error("Add exit event failed");
            return;
        }

        if (event_base_dispatch(base.get()) < 0) {
            tools::log::error("Run dispatcher failed");
        }

        wc.event_upload.reset();
        wc.watchdog.reset();
        ecdh.reset();
        ctx.reset();
        http_server.reset();
        base.reset();

#   ifdef __GNUG__
        close(fd);
#   elif _WIN32
        closesocket(fd);
#   endif
    }

    void _process_request(struct evhttp_request* req, uint64_t worker_ix) {

        SDK_CP_ASSERT(worker_ix < __num_threads);

        if (evhttp_cmd_type::EVHTTP_REQ_OPTIONS == req->type) {
            evhttp_add_header(req->output_headers, "Server", k_server_name);
            evhttp_add_header(req->output_headers, "Access-Control-Allow-Origin",   "*");
            evhttp_add_header(req->output_headers, "Access-Control-Allow-Headers",  "origin, content-type, accept");
            evhttp_add_header(req->output_headers, "Access-Control-Max-Age",        "3628800");
            evhttp_add_header(req->output_headers, "Access-Control-Allow-Methods",  "PUT,DELETE,GET,POST");

            tools::log::trace("Received [OPTIONS].");
            evhttp_send_reply(req, HTTP_NOCONTENT, "No Content", nullptr/*evb.get()*/);
            return;
        }

        auto in_buf     = evhttp_request_get_input_buffer(req);
        auto in_buf_len = evbuffer_get_length(in_buf);

        if (0 == in_buf_len) {
            tools::log::trace("Received [EMPTY] data.");
            evhttp_send_reply(req, HTTP_NOCONTENT, "No Content", nullptr/*evb.get()*/);
            return;
        }

        auto& wc                = _worker_context[worker_ix];
        auto& data_compressed   = wc.request_data;
        auto& uncompressed_data = wc.uncompressed_data;

        bool data_is_compressed = false;
        struct evkeyvalq *header = evhttp_request_get_input_headers(req);
        struct evkeyval* kv = header->tqh_first;
        while (kv) {
            if (evutil_ascii_strcasecmp(kv->key, "Accept-Compression") == 0) {
                if (evutil_ascii_strcasecmp(kv->value, "lzo") == 0) {
                    data_is_compressed = true;
                    break;
                }
                tools::log::warning("Request with unsupported compression. Skip. Header: ", kv->key, ":", kv->value);
                evhttp_send_error(req, HTTP_BADREQUEST, "Unsupported compression type.");
                return;
            }
            kv = kv->next.tqe_next;
        }

        server_call_context<server<__request_handler, __num_threads>> call_context(wc.request, wc.response, req, shared_from_this(), worker_ix, wc.direct_data, wc.stack);

        data_compressed.resize(in_buf_len);
        evbuffer_copyout(in_buf, &data_compressed[0], in_buf_len);

        char *client_ip = nullptr; u_short client_port = 0;
        evhttp_connection_get_peer(evhttp_request_get_connection(req), &client_ip, &client_port);

        tools::log::trace("Received data ", "[ size=", in_buf_len, ", addr=", (client_ip != nullptr ? client_ip : "no_data"), ":", client_port, " ]:\n", data_compressed);

        char const* data;
        size_t data_size;
        if (data_is_compressed) {
            auto const compressed_data_ptr = &data_compressed[0];
            auto const compressed_data_sz  = data_compressed.size();

            uncompressed_data.resize(4*1024*1024);
            auto uncompressed_data_ptr    = &uncompressed_data[0];
            lzo_uint uncompressed_data_sz = uncompressed_data.size();

            auto const result = lzo1x_decompress_safe(
                compressed_data_ptr,
                compressed_data_sz,
                uncompressed_data_ptr,
                &uncompressed_data_sz,
                &wc.lzo_tmp_memory[0]
            );
            
            if (result != LZO_E_INPUT_OVERRUN) {
                tools::log::error("Decompression failed. Big request.");
                evhttp_send_error(req, HTTP_BADREQUEST, "Big request.");
                return;
            }

            if (result != LZO_E_OK) {
                tools::log::error("Decompression failed [ ", result, " ]");
                evhttp_send_error(req, HTTP_BADREQUEST, "Decompression failed.");
                return;
            }

            data        = (char const*)&uncompressed_data[0];
            data_size   = uncompressed_data_sz;
        } else {
            data        = (char const*)&data_compressed[0];
            data_size   = data_compressed.size();
        }

        try {
            Json::Reader reader;
            if (!reader.parse(data, &data[data_size], call_context.request)) {
                types::string str(data, &data[data_size]);
                tools::log::error("Parse json failed. Incorrect data: ", str.c_str());
                evhttp_send_error(req, HTTP_BADREQUEST, "No json data");
                return;
            }

            call_context.response.clear();
            call_context.direct_data.clear();
            wc.stack.clear();

            if (!wc.rh->on_request(call_context)) {
                tools::log::error("Process request failed. No request handler for method.");
                evhttp_send_error(req, HTTP_BADMETHOD, "No request handler for current method.");
                return;
            }
        }
        catch (std::exception& e) {
            tools::log::error("Exception. Worker error. Description=", e.what());
            evhttp_send_error(req, HTTP_INTERNAL, "Internal error.");
            return;
        }
        catch (...) {
            tools::log::error("Exception. Worker error.");
            evhttp_send_error(req, HTTP_INTERNAL, "Internal error.");
            return;
        }

        if (call_context.hold) {
            evhttp_request_own(req);
            tools::log::trace("Request was owned for future handling.");
            return;
        }

        auto buffer_deleter = [](evbuffer* obj) { if (nullptr != obj) evbuffer_free(obj); };
        std::unique_ptr<evbuffer, decltype(buffer_deleter)> evb(evbuffer_new(), buffer_deleter);

        uint8_t const*  out_data = nullptr;
        size_t          out_size = 0;

        if (!call_context.direct_data.empty()) {
            if (!_compress_response(worker_ix, call_context.direct_data, out_data, out_size, req->output_headers)) {
                evhttp_send_error(req, HTTP_INTERNAL, "Compression failed.");
                return;
            }
        }
        else if (!call_context.response.empty()) {
            wc.response_str.clear();
            wc.response_str.reserve(1024);

            tools::json_writer<std::string> writer;
            writer.write(call_context.response, wc.response_str);

            if (!_compress_response(worker_ix, wc.response_str, out_data, out_size, req->output_headers)) {
                evhttp_send_error(req, HTTP_INTERNAL, "Compression failed.");
                return;
            }
        }

        if (0 == out_size) {
            evhttp_send_reply(req, HTTP_NOCONTENT, "OK", nullptr);
            return;
        }

        evbuffer_add(evb.get(), out_data, out_size);
        evhttp_send_reply(req, HTTP_OK, "OK", evb.get());
    }

    template<typename T> inline std::shared_ptr<T> _make_shared(T* t, void(*f)(T*)) {
        return std::shared_ptr<T>(t, [f](T* obj) {
            if (nullptr != obj) {
                f(obj);
            }
        });
    }

    struct uploader_context {
        using function_type = std::function<bool(server_call_context<server<__request_handler, __num_threads>>&)>;

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

    char const* const   _certificate;
    char const* const   _private_key;
    const bool          _compression;
    const bool          _https;

public:
    server(
        std::shared_ptr<__request_handler> (&rh)[__num_threads],
        char const* const certificate = nullptr,
        char const* const private_key = nullptr,
        const bool https = true,
        const bool compression = false
    ) 
        : _https(https)
        , _compression(compression)
        , _certificate(certificate)
        , _private_key(private_key)

    {
        for (uint64_t ix = 0; ix < __num_threads; ++ix) {
            auto& wc = _worker_context[ix];
            wc.rh = rh[ix];
        }
    }
    ~server() {
        stop();
    }
    void run(std::string const& host, uint64_t const port) {
        if (host.empty() || 0 == port) {
            tools::log::error("Invalid argument");
            return;
        }
        evthread_use_windows_threads();

        tools::log::info("Start server at: host=", host.c_str(), ", port=", port);
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

    void register_uploader(server_call_context<server<__request_handler, __num_threads>> const& cc, std::function<bool(server_call_context<server<__request_handler, __num_threads>>&)>&& f) {
        auto& wc = _worker_context[cc.request_handler_ix];
        {
            tools::threading::spin_blocker_infinite guard(wc.uploaders_cs);
            wc.uploaders.emplace_back(std::forward<std::function<bool(server_call_context<server<__request_handler, __num_threads>>&)>>(f), cc.handle);
        }
        event_active(wc.event_upload.get(), EV_UPLOAD, 0);
    }

    void stop() {
        for (auto& wc : _worker_context) {
            //closesocket(wc.fd);
            if (!!wc.watchdog) {
                event_active(wc.watchdog.get(), EV_READ | EV_WRITE, 1);
            }
        }
        for (uint64_t ix = 0; ix < __num_threads; ++ix) {
            auto& wc = _worker_context[ix];
            if (!!wc.worker_thread) {
                wc.worker_thread->join();
                wc.worker_thread.reset();
            }
        }
    }
};
