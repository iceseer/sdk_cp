/**
 *
 * filename: tools.h
 * summary:
 * author: caosiyang
 * email:  csy3228@gmail.com
 *
 */
#ifndef TOOLS_H
#define TOOLS_H

#include <stdio.h>
#include <stdint.h>
#include <iostream>
using namespace std;


#define LOG(format, ...) __LOG<5*1024>(format, __VA_ARGS__)
template<uint64_t __buffer, typename...__args> SDK_CP_ALWAYS_INLINE void __LOG(char const* const format, __args const&...args) {
    char buffer[__buffer];
    sprintf_s(buffer, format, args...);
    buffer[SDK_CP_ARRAY_SIZE(buffer) - 1] = 0;
    tools::log::trace("[WS] ", buffer);
}

inline uint16_t myhtons(uint16_t n) {
	return ((n & 0xff00) >> 8) | ((n & 0x00ff) << 8);
}


inline uint16_t myntohs(uint16_t n) {
	return ((n & 0xff00) >> 8) | ((n & 0x00ff) << 8);
}


inline uint32_t myhtonl(uint32_t n) {
	return ((n & 0xff000000) >> 24) | ((n & 0x00ff0000) >> 8) | ((n & 0x0000ff00) << 8) | ((n & 0x000000ff) << 24);
}


inline uint32_t myntohl(uint32_t n) {
	return ((n & 0xff000000) >> 24) | ((n & 0x00ff0000) >> 8) | ((n & 0x0000ff00) << 8) | ((n & 0x000000ff) << 24);
}


inline uint64_t myhtonll(uint64_t n) {
	return (uint64_t)myhtonl((uint32_t)(n >> 32ull)) | ((uint64_t)myhtonl(uint32_t(n)) << 32ull);
}


inline uint64_t myntohll(uint64_t n) {
	return (uint64_t)myntohl((uint32_t)(n >> 32ull)) | ((uint64_t)myntohl(uint32_t(n)) << 32ull);
}


#endif
