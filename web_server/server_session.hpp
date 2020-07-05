uint64_t const k_read_buff_size  = 128;
uint64_t const k_write_buff_size = k_read_buff_size * 8;

struct connection_ctx_t final {
    evutil_socket_t fd;
    event_base*     base;
    event*          read_event;
    event*          write_event;

    uint8_t read_buff [k_read_buff_size];
    uint8_t write_buff[k_write_buff_size];

    uint64_t read_buff_used;
    uint64_t write_buff_used;

    connection_ctx_t() : base(nullptr), read_event(nullptr), write_event(nullptr), read_buff_used(0), write_buff_used(0) {

    }

    ~connection_ctx_t() {
        tools::log::trace("Context delete: fd=", fd);

        if(nullptr != read_event) {
            event_del (read_event);
            event_free(read_event);
        }

        if(nullptr != write_event) {
            event_del (write_event);
            event_free(write_event);
        }

        close(fd);
    }
};
