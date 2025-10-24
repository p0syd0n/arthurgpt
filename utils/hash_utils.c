#pragma once

#include <stdint.h>
#include <string.h>

/// 64-bit FNV-1a — simple, fast, decent avalanche
static inline uint64_t hash_bytes(const void *data, size_t len) {
    const uint8_t *p = data;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; i++)
        h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}

/// Hash a string (null-terminated)
static inline uint64_t hash_str(const char *s) {
    return hash_bytes(s, strlen(s));
}

/// Mix two 64-bit hashes (order-sensitive)
static inline uint64_t mix(uint64_t a, uint64_t b) {
    a ^= b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2);
    return a;
}
