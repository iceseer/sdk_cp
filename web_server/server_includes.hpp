#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#ifdef __GNUG__
#   include <arpa/inet.h>
#   include <netinet/in.h>
#   include <sys/socket.h>
#elif _WIN32
#   include <winsock2.h>
#   include <stdio.h>
#   include <windows.h>
#   pragma comment(lib, "Ws2_32.lib")
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <errno.h>
#include <event2/event.h>
#include <evhttp.h>
#include <event2/thread.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/listener.h>
#include <event2/event-config.h>
#include <event2/util.h>


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jsoncpp/include/json.h>
#include "../compression/minilzo.h"

#include "../tools/tools.hpp"
namespace server {
#   include "ws/websocket.h"
#   include "ws/connection.h"
#   include "server_defines.hpp"

#ifdef __GNUG__
#   include "server_linux.hpp"
#elif _WIN32
    struct user_t;

#   include "server_win.hpp"
#   include "server_ws_user_win.hpp"
#   include "server_ws_win.hpp"
#endif

}

namespace tools {
    template<typename __string_type> SDK_CP_INLINE __string_type& str_conv(server::frame_buffer_t const &from, __string_type& to) {
        to += "[LEN: ";
        str_conv(from.len, to);

        to += "], [DATA:";
        if (nullptr != from.data)
            to += from.data;
        
        to += "], [STR_DATA:";
        if (nullptr != &from.str_data)
            to += from.str_data;
        to += "]";

        return to;
    }
}

#endif//__SERVER_HPP__
