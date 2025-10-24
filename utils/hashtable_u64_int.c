#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Optimized: 16 bytes, no padding, no separate 'used' flag
typedef struct {
    uint64_t key;    // 8 bytes (0 means unused)
    int value;       // 4 bytes
    uint32_t _pad;   // 4 bytes padding for alignment
} EntryU64Int;

typedef struct {
    size_t size;
    size_t mask;  // size - 1, for fast modulo via bitwise AND
    EntryU64Int *entries;
} HashU64Int;

static inline HashU64Int *create_u64_int(size_t size) {
    // Ensure size is power of 2
    // If you pass 300000, this will round up to 524288 (2^19)
    size_t actual_size = 1;
    while (actual_size < size) {
        actual_size <<= 1;
    }
    
    HashU64Int *ht = malloc(sizeof(HashU64Int));
    ht->size = actual_size;
    ht->mask = actual_size - 1;
    ht->entries = calloc(actual_size, sizeof(EntryU64Int));
    return ht;
}

static inline void free_u64_int(HashU64Int *ht) {
    free(ht->entries);
    free(ht);
}

static inline int *get_u64_int(HashU64Int *ht, uint64_t key) {
    if (key == 0) return NULL;
    size_t i = key & ht->mask;
    
    for (size_t n = 0; n < ht->size; n++) {
        EntryU64Int *e = &ht->entries[i];
        if (e->key == 0) return NULL;
        if (e->key == key) return &e->value;
        i = (i + n*n) & ht->mask;  // Quadratic: n², instead of just n
    }
    return NULL;
}

static inline void set_u64_int(HashU64Int *ht, uint64_t key, int value) {
    // Reserve key=0 for "empty"
    if (key == 0) return;
    
    size_t i = key & ht->mask;  // Fast bitwise AND instead of modulo
    
    for (size_t n = 0; n < ht->size; n++) {
        EntryU64Int *e = &ht->entries[i];
        if (e->key == 0 || e->key == key) {
            e->key = key;
            e->value = value;
            return;
        }
        i = (i + 1) & ht->mask;  // Fast increment with wraparound
    }
    // Table is full - this shouldn't happen if sized correctly
}

static inline void clear_u64_int(HashU64Int *ht) {
    // Fast memset instead of loop
    memset(ht->entries, 0, ht->size * sizeof(EntryU64Int));
}