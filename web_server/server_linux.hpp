template<typename __request_handler, uint32_t __num_threads> class server final {

    struct net_header {
        enum { k_header_data_size = 5 };
        enum { k_header_size = k_header_data_size + 4 };

        struct {
            uint8_t  flags;
            uint32_t size;
        }        data;
        uint32_t crc32;
    };

    static void on_request(struct evhttp_request* req, void* arg) {
        if (nullptr != arg && nullptr != req) {
            auto __this = (server*)arg;
            __this->_process_request(req);
        } else {
            tools::log::error("On request argument is null. Send error. Request=", (uintptr_t)req, ", arg=", (uintptr_t)arg);
            evhttp_send_error(req, HTTP_INTERNAL, "Internal error(no argument)");
        }
    }

    static void on_worker(server* __this, uint64_t ix) {
        try {
            tools::log::info("Worker thread \033[1;32mSTARTED\033[0m: ix = ", ix);
            __this->_worker_proc(ix);
        } catch(std::exception& e) {
            tools::log::error("Exception. Worker error. Description=", e.what());
        } catch(...) {
            tools::log::error("Exception. Worker error.");
        }

        tools::log::info("Worker thread \033[1;32mSTOPED\033[0m: ix = ", ix);
    }

    static void _exit_ev_callback(evutil_socket_t sock, short what, void *arg) {
        struct event_base *base = (event_base*)arg;
        event_base_loopbreak(base);
    }

    void _worker_proc(uint64_t const ix) {
        auto& wc = _worker_context[ix];
        wc.lzo_tmp_memory.resize(LZO1X_1_MEM_COMPRESS);

        auto& base = wc.base;
        base = _make_shared(event_base_new(), &event_base_free);
        if (!base) {
            tools::log::error("Event base creation failed: err=", strerror(errno));
            return;
        }

        auto http_server = _make_shared(evhttp_new(base.get()), &evhttp_free);
        if (!http_server) {
            tools::log::error("Http-server creation failed: err=", strerror(errno));
            return;
        }

        signal(SIGPIPE, SIG_IGN);

        if (evhttp_accept_socket(http_server.get(), _fd) == -1) {
            tools::log::error("Http accept socket failed: err=", strerror(errno));
            return;
        }

        evhttp_set_gencb(http_server.get(), on_request, this);

        if (lzo_init() != LZO_E_OK) {
            tools::log::error("LZO init failed.");
            return;
        }

        if (event_base_dispatch(base.get()) < 0) {
            tools::log::error("Run dispatcher failed");
        }

        http_server.reset();
        base.reset();
    }

    void _process_request(struct evhttp_request* req) {

        //todo: сюда можно передать ix
        uint64_t worker_ix = 0;
        for (; worker_ix < __num_threads; ++worker_ix) {
            if (std::this_thread::get_id() == _worker_context[worker_ix].worker_thread->get_id()) {
                break;
            }
        }

        SDK_CP_ASSERT(worker_ix < __num_threads);

        auto in_buf     = evhttp_request_get_input_buffer(req);
        auto in_buf_len = evbuffer_get_length(in_buf);

        if (in_buf_len > net_header::k_header_size) {
            auto& wc                = _worker_context[worker_ix];
            auto& data_compressed   = wc.request_data;
            auto& request           = wc.request;
            auto& response          = wc.response;
            auto& uncompressed_data = wc.uncompressed_data;

            data_compressed.resize(in_buf_len);
            evbuffer_copyout(in_buf, &data_compressed[0], in_buf_len);

            tools::log::trace("Received data ",
                "[ size:", in_buf_len,
            " ]");

#ifdef KIDZZ_USE_EXTENDED_PROTOCOL_FORMAT
            net_header nh;
            nh.data.flags = data_compressed[0];
            nh.data.size  = *(uint32_t*)&data_compressed[1];
            nh.crc32      = *(uint32_t*)&data_compressed[5];

            if (nh.data.size > k_server_max_request_size) {
                tools::log::warning("Too large request. Skip.");
                evhttp_send_error(req, HTTP_NOCONTENT, "Too large request");
                return;
            }

            auto const hash = tools::hash::fnv1a_hash_bytes_32(&data_compressed[0], net_header::k_header_data_size);
            if (nh.crc32 != hash) {
                tools::log::error("Incorrect data. Parsing failed.");
                evhttp_send_error(req, HTTP_NOCONTENT, "No correct data");
                return;
            }

            char const* data;
            auto const data_size = nh.data.size;
            if (nh.data.flags & SERVER_FLAG_COMPRESSED) {
                auto const compressed_data_ptr = &data_compressed[net_header::k_header_size];
                auto const compressed_data_sz  = data_compressed.size() - net_header::k_header_size;

                uncompressed_data.resize(nh.data.size);
                auto uncompressed_data_ptr = &uncompressed_data[0];
                lzo_uint uncompressed_data_sz = nh.data.size;

                auto const result = lzo1x_decompress(
                    compressed_data_ptr,
                    compressed_data_sz,
                    uncompressed_data_ptr,
                    &uncompressed_data_sz,
                    NULL
                );
                if (result != LZO_E_OK) {
                    tools::log::error("Decompression failed [ ", result, " ]");
                    evhttp_send_error(req, HTTP_NOCONTENT, "Decompression failed.");
                    return;
                }

                data = (char const*)&uncompressed_data[0];
            } else {
                data = (char const*)&data_compressed[net_header::k_header_size];
            }
#else//KIDZZ_USE_EXTENDED_PROTOCOL_FORMAT
            char const* data     = (char const*)&data_compressed[0];
            auto const data_size = data_compressed.size();
#endif//KIDZZ_USE_EXTENDED_PROTOCOL_FORMAT

            Json::Reader reader;
            if (!reader.parse(data, &data[data_size], request)) {
                types::string str(data, &data[data_size]);
                tools::log::error("Parse json failed. Incorrect data: ", str.c_str());
                evhttp_send_error(req, HTTP_NOCONTENT, "No json data");
                return;
            }

            response.clear();
            if (!wc.rh->on_request(request, response)) {
                tools::log::error("Process request failed. No request handler for method.");
                evhttp_send_error(req, HTTP_BADMETHOD, "No request handler for current method.");
                return;
            }

            auto buffer_deleter = [](evbuffer* obj) { if (nullptr != obj) evbuffer_free(obj); };
            std::unique_ptr<evbuffer, decltype(buffer_deleter)> evb(evbuffer_new(), buffer_deleter);

            if (!response.empty()) {
                wc.response_str.clear();
                wc.response_str.reserve(1024);

                tools::json_writer<std::string> writer;
                writer.write(response, wc.response_str);

                //Json::FastWriter fastWriter;
                //auto response_str = fastWriter.write(response);

#ifdef KIDZZ_USE_EXTENDED_PROTOCOL_FORMAT
                data_compressed.resize(net_header::k_header_size + response_str.size() + response_str.size() / 16 + 64 + 3);
                lzo_uint compressed_data_sz = 0;

                auto const result = lzo1x_1_compress(
                    (uint8_t const*)response_str.c_str(),
                    response_str.size(),
                    &data_compressed[net_header::k_header_size],
                    &compressed_data_sz,
                    &wc.lzo_tmp_memory[0]
                );

                data_compressed[0]              = SERVER_FLAG_COMPRESSED;
                *(uint32_t*)&data_compressed[1] = response_str.size();

                if (result != LZO_E_OK) {
                    tools::log::error("Compression failed [ ", result, " ]");
                    evhttp_send_error(req, HTTP_INTERNAL, "Compression failed.");
                    return;
                }

                evbuffer_add(evb.get(), &data_compressed[0], compressed_data_sz + net_header::k_header_size);
#else//KIDZZ_USE_EXTENDED_PROTOCOL_FORMAT
                evbuffer_add_printf(evb.get(), "%s", response_str.c_str());
#endif//KIDZZ_USE_EXTENDED_PROTOCOL_FORMAT
            }

            evhttp_add_header(req->output_headers, "Server", k_server_name);
            evhttp_add_header(req->output_headers, "Connection", "close");
            evhttp_add_header(req->output_headers, "Content-Type","application/json");
            evhttp_send_reply(req, HTTP_OK, "OK", evb.get());
        } else {
            tools::log::error("Request with empty data.");
            evhttp_send_error(req, HTTP_NOCONTENT, "No data");
        }
    }

    template<typename T> inline std::shared_ptr<T> _make_shared(T* t, void(*f)(T*)) {
        return std::shared_ptr<T>(t, [f](T* obj) {
            if (nullptr != obj) {
                f(obj);
            }
        });
    }

    struct {
        std::shared_ptr<__request_handler> rh;
        std::vector<uint8_t>               request_data;
        std::vector<uint8_t>               uncompressed_data;
        std::vector<uint8_t>               lzo_tmp_memory;

        std::string                        response_str;
        Json::Value                        request;
        Json::Value                        response;
        std::unique_ptr<std::thread>       worker_thread;

        std::shared_ptr<event_base>        base;
    } _worker_context[__num_threads];

    evutil_socket_t             _fd;

public:
    server(std::shared_ptr<__request_handler> (&rh)[__num_threads]) : _fd(-1) {
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

        for (auto& wc : _worker_context) {
            if (!wc.rh) {
                tools::log::error("Some of request handlers not defined.");
                return;
            }
        }

        stop();
        tools::log::info("Start server at: host=", host.c_str(), ", port=", port);

        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_fd < 0) {
            tools::log::error("Create socket failed: err=", strerror(errno));
            return;
        }

        if (evutil_make_socket_nonblocking(_fd) < 0) {
            tools::log::error("Set socket nonblocking failed: err=", strerror(errno));
            return;
        }

        struct sockaddr_in sin;
        sin.sin_family      = AF_INET;
        sin.sin_port        = htons(port);
        sin.sin_addr.s_addr = inet_addr(host.c_str());
        int on = 1;

        if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
            tools::log::error("Set socket option SO_REUSEADDR failed: err=", strerror(errno));
            return;
        }

        if (bind(_fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
            tools::log::error("Bind socket failed: err=", strerror(errno));
            return;
        }

        if (listen(_fd, k_listen_sockets_count) < 0) {
            tools::log::error("Listen socket failed: err=", strerror(errno));
            return;
        }

        for (uint64_t ix = 0; ix < __num_threads; ++ix) {
            auto& wc = _worker_context[ix];
            wc.worker_thread.reset(new std::thread(&on_worker, this, ix));
        }
    }

    void stop() {
        for (auto& wc : _worker_context) {
            if (!!wc.base) {
                event_base_loopbreak(wc.base.get());
            }
            if (!!wc.worker_thread) {
                wc.worker_thread->join();
                wc.worker_thread.reset();
            }
        }
        close(_fd);
    }
};
