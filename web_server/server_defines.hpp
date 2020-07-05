///////////////////////////
// author: Alexander Lednev
// date: 28.11.2018
///////////////////////////

#define k_server_name "AMTS_WebAPI"
//#define KIDZZ_USE_EXTENDED_PROTOCOL_FORMAT

//===== SERVER SIGNALS
#define EV_UPLOAD 0x0100

//===== WEB PACKET FLAGS
uint8_t const  SERVER_FLAG_COMPRESSED = 0x01;

//===== CONSTANTS
uint64_t const k_server_requests_count              = 1 * 1000 * 1000;
uint64_t const k_server_wait_in_requests_timeout_ms = 50;
uint64_t const k_server_max_request_size            = 1 * 1024 * 1024;

uint64_t const k_server_cache_size    = 100ull;
int const k_listen_sockets_count      = 1000;

typedef tools::stack_allocator<5 * 1024 * 1024> addition_stack;

//static void* SDK_CP_global_calloc(size_t size) {
//    return calloc(1, size);
//}

SDK_CP_INLINE void SDK_CP_global_init_SSL() {
    //CRYPTO_set_mem_functions(&SDK_CP_global_calloc, &realloc, &free);
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    tools::log::info("[OPENSSL] version \"", SSLeay_version(SSLEAY_VERSION), "\"\n[LIBEVENT] version \"", event_get_version(), "\"");
}
