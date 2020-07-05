///////////////////////////
// author: Alexander Lednev
// date: 27.08.2018
///////////////////////////

#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#include "../tools/tools.hpp"

namespace memory {
    typedef tools::allocators::heap<SDK_CP_DEFAULT_HEAP_CHUNK> heap_type; 
    extern heap_type g_heap;
}

template<typename T, typename...A> inline T* g_new(A const&...a) {
    auto* ptr = memory::g_heap.allocate(sizeof(T));
    return new(ptr) T(a...);
}
template<typename T> inline void g_delete(T* t) {
    if (nullptr != t) {
        t->~T();
        memory::g_heap.deallocate((uint8_t*)t);
    }
}

#endif//__MEMORY_HPP__