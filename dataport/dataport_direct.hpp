class dataport_direct final {
    struct message_header {
        uint64_t hash;
        uint64_t size; // data
    };
    struct message {
        message_header header;
        data_type*     data_ref;
    };
    struct file_header {
        uint64_t size;
        uint64_t file_version;
    };
    static_assert(offsetof(message, header) == 0, "incorrect header position");

private:
    std::string           _db_name;
    std::shared_ptr<FILE> _database;

public:
    dataport_direct(dataport_direct const&) = delete;
    dataport_direct& operator=(dataport_direct const&) = delete;

private:
    inline uint64_t _data_hash(data_type const& data) {
        return tools::hash::fnv1a_hash_bytes(&data[0], data.size());
    }

    inline edata_error _data_format(message const& msg, data_type& out) {
        uint64_t const msg_size = sizeof(msg.header) + msg.data_ref->size();
        out.resize(msg_size);

        memcpy(&out[0], &msg.header, sizeof(msg.header));
        memcpy(&out[sizeof(msg.header)], &(*msg.data_ref)[0], msg.data_ref->size());

        return edata_error::ok;
    }

private:
    inline edata_error _database_open() {
        if (nullptr != _database) { return edata_error::ok; }
        _database.reset(fopen(_db_name.c_str(), "a+"), [](FILE* file) {
            if (nullptr != file) {
                fclose(file);
            }
        });
        return (!_database ? edata_error::open_file_failed : edata_error::ok);
    }

    inline FILE* _database_handle() {
        return _database.operator->();
    }

    inline address _database_end_position() {
        _fseeki64(_database_handle(), 0, SEEK_END);
        return ftell(_database_handle());
    }

    inline edata_error _database_write(address& addr, message const& msg) {
        std::shared_ptr<data_type> format_buffer = global_get_from_cache<data_type>();

        format_buffer->resize(sizeof(msg.header));
        auto& header = *(message_header*)&(*format_buffer)[0];

        header.size = msg.data_ref->size();
        header.hash = _data_hash(*msg.data_ref);

        edata_error result = _data_format(msg, *format_buffer);
        if (edata_error::ok == result) {
            addr = _database_end_position();
            if (1 != fwrite(&(*format_buffer)[0], format_buffer->size(), 1, _database_handle())) {
                result = edata_error::write_operation_failed;
            }
        }
        return result;
    }

    inline edata_error _database_read(address& addr, message& msg) {
        edata_error result = edata_error::set_file_position_failed;
        if (0 == _fseeki64(_database_handle(), addr, SEEK_SET)) {
            result = edata_error::read_operation_failed;
            if (1 == fread(&msg.header, sizeof(msg.header), 1, _database_handle())) {
                try {
                    msg.data_ref->resize(msg.header.size);
                    if (1 == fread(&(*msg.data_ref)[0], msg.header.size, 1, _database_handle())) {
                        auto const hash = _data_hash(*msg.data_ref);
                        if (hash == msg.header.hash) {
                            result = edata_error::ok;
                        } else {
                            result = edata_error::invalid_hash;
                        }
                    }
                } catch(std::exception& /*e*/) {
                    result = edata_error::memory_resize_failed;
                }
            }
        }
        return result;
    }

public:
    dataport_direct(std::string const& db_name) : _db_name(db_name) { }
    dataport_direct(char const* db_name) : _db_name(db_name) { }

    ~dataport_direct() {

    }

    edata_error add(address& addr, data_type& data) {
        edata_error result = edata_error::invalid_arg;
        if (!data.empty()) {
            result = _database_open();
            if (edata_error::ok == result) {
                message msg = {{ 0, 0 }, &data};
                result = _database_write(addr, msg);
            }
        }
        return result;
    }

    edata_error get(address addr, data_type& data) {
        edata_error result = edata_error::invalid_arg;
        if (0 != addr) {
            result = _database_open();
            if (edata_error::ok == result) {
                message msg = {{ 0, 0 }, &data};
                result = _database_read(addr, msg);
            }
        }
        return result;
    }
};
typedef std::shared_ptr<dataport_direct> dataport_ptr;
