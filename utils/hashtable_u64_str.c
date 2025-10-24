#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t key;
    char *value;
    uint8_t used;
} EntryU64Str;

typedef struct {
    size_t size;
    EntryU64Str *entries;
} HashU64Str;

static inline HashU64Str *create_u64_str(size_t size) {
    HashU64Str *ht = malloc(sizeof(HashU64Str));
    ht->size = size;
    ht->entries = calloc(size, sizeof(EntryU64Str));
    return ht;
}

static inline void free_u64_str(HashU64Str *ht) {
    for (size_t i = 0; i < ht->size; i++)
        if (ht->entries[i].used) free(ht->entries[i].value);
    free(ht->entries);
    free(ht);
}

static inline char *get_u64_str(HashU64Str *ht, uint64_t key) {
    size_t i = key % ht->size;
    for (size_t n = 0; n < ht->size; n++) {
        EntryU64Str *e = &ht->entries[i];
        if (!e->used) return NULL;
        if (e->key == key) return e->value;
        i = (i + 1) % ht->size;
    }
    return NULL;
}

static inline void set_u64_str(HashU64Str *ht, uint64_t key, const char *value) {
    size_t i = key % ht->size;
    for (size_t n = 0; n < ht->size; n++) {
        EntryU64Str *e = &ht->entries[i];
        if (!e->used || e->key == key) {
            e->key = key;
            e->value = strdup(value);
            e->used = 1;
            return;
        }
        i = (i + 1) % ht->size;
    }
}
