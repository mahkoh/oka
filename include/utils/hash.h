#pragma once

#include <stdbool.h>

#include "utils/utils.h"

struct hash_map_ops {
    u32 (*hash)(void *key);
    void *(*key)(void *val);
    bool (*equal)(void *key1, void *key2);
};

struct hash_map;

struct hash_map *hash_map_new(struct hash_map_ops *ops);
void hash_map_free(struct hash_map *map);
void *hash_map_get(struct hash_map *map, void *key);
void *hash_map_remove(struct hash_map *map, void *key);
void hash_map_reserve(struct hash_map *map, size_t num);
void hash_map_set(struct hash_map *map, void *val);

u32 hash_str(const char *s);
u32 hash_mem(void *mem_, size_t len);

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
