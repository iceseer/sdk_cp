typedef int64_t address;
typedef std::vector<uint8_t> data_type;

uint64_t const edataport_cache_size = 0x80;
uint64_t const database_version     = 0x0000000000000001;

enum struct edata_error {
    ok = 0,
    open_file_failed,
    invalid_arg,
    write_operation_failed,
    read_operation_failed,
    memory_resize_failed,
    invalid_hash,
    set_file_position_failed,
};

typedef ::tools::objects_cache_manager<edataport_cache_size, data_type> cache_type;
extern cache_type global_cache;

template<typename T> inline std::shared_ptr<T> global_get_from_cache() {
    std::shared_ptr<T> t;
    static_cast<::tools::objects_cache<edataport_cache_size, T>*>(&global_cache)->get_cached_object(t);
    return t;
}