#ifndef __TOOLS_HPP__
#define __TOOLS_HPP__

#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <ratio>
#include <deque>
#include <queue>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <mutex>
#include <list>
#include <vector>
#include <algorithm>
#include <random>
#include <codecvt>
#include <locale>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#ifdef __GNUG__
#   define _POSIX_SOURCE
#   include <unistd.h>
#   undef _POSIX_SOURCE
#elif _WIN32
#   include <direct.h>
#   include <filesystem>
#endif

#include <openssl/md5.h>
#include <jsoncpp/include/json.h>

#ifdef _WIN32
#   include <time.h>
#endif//_WIN32

#   ifdef __GNUG__
#   include <unistd.h>
#   elif _WIN32
#   include <windows.h>
#   endif

#include "../sdk_defines.hpp"

namespace tools {
#   include "processor.hpp"
#   include "time.hpp"
#   include "random.hpp"
#   include "hash.hpp"
#   include "memory.hpp"
#   include "file.hpp"
#   include "list.hpp"
#   include "string_converter.hpp"
#   include "binary_serializer.hpp"
#   include "string_utils.hpp"

//  threading
#   include "wait_for_single_object.hpp"
#   include "blocker.hpp"
#   include "thread_pool.hpp"

//  allocators
#   include "pool_allocator.hpp"
#   include "stack_allocator.hpp"
#   include "heap_allocator.hpp"
#   include "heap.hpp"

//  collections
#   include "static_frame.hpp"
#   include "dynamic_frame.hpp"
#   include "ring_buffer.hpp"
#   include "linear_hash_map.hpp"

//  tools
#   include "threaded_handler.hpp"
#   include "cache.hpp"
#   include "log.hpp"
#   include "callback_keeper.hpp"
#   include "json_serializer.hpp"
#   include "json_writer.hpp"

// algorithm
#   include "point.hpp"
#   include "b_spline.hpp"
//#   include "linea.hpp"

}

#endif//__TOOLS_HPP__
