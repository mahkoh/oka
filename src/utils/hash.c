#include <stddef.h>

#include "utils/hash.h"
#include "utils/utils.h"
#include "utils/xmalloc.h"

static char hash_map_deleted;

struct hash_map {
    void **bucket;
    size_t buckets;
    size_t elements;
    size_t deleted;
    const struct hash_map_ops *ops;
};

enum hash_map_bucket_status {
    HASH_MAP_BUCKET_SET,
    HASH_MAP_BUCKET_EMPTY,
    HASH_MAP_BUCKET_DELETED,
};

struct hash_map *hash_map_new(struct hash_map_ops *ops)
{
    auto map = xnew(struct hash_map);
    map->bucket = xnew_array(void *, 2);
    map->bucket[0] = NULL;
    map->bucket[1] = NULL;
    map->buckets = 2;
    map->elements = 0;
    map->deleted = 0;
    map->ops = ops;
    return map;
}

void hash_map_free(struct hash_map *map)
{
    free(map->bucket);
    free(map);
}

static enum hash_map_bucket_status hash_map_search(struct hash_map *map, void *key,
        size_t *pos)
{
    auto hash = map->ops->hash(key);
    auto idx = (size_t)hash & (map->buckets - 1);
    size_t del_pos = 0;
    bool del_pos_set = false;
    for (size_t i = 1; ; idx += i, i++) {
        auto bucket = map->bucket[idx];
        if (!bucket) {
            if (del_pos_set) {
                *pos = del_pos;
                return HASH_MAP_BUCKET_DELETED;
            }
            *pos = idx;
            return HASH_MAP_BUCKET_EMPTY;
        }
        if (bucket == &hash_map_deleted && !del_pos_set) {
            del_pos = idx;
            del_pos_set = true;
        } else {
            auto key2 = map->ops->key(bucket);
            if (map->ops->equal(key2, key)) {
                *pos = idx;
                return HASH_MAP_BUCKET_SET;
            }
        }
    }
}

void *hash_map_get(struct hash_map *map, void *key)
{
    size_t pos;
    if (hash_map_search(map, key, &pos) == HASH_MAP_BUCKET_SET)
        return map->bucket[pos];
    return NULL;
}

void *hash_map_remove(struct hash_map *map, void *key)
{
    size_t pos;
    if (hash_map_search(map, key, &pos) == HASH_MAP_BUCKET_SET) {
        auto val = map->bucket[pos];
        map->bucket[pos] = &hash_map_deleted;
        map->deleted++;
        return val;
    }
    return NULL;
}

static void hash_map_copy(struct hash_map *map, void **table, size_t buckets)
{
    for (size_t i = 0; i < buckets; i++)
        table[i] = NULL;

    auto elements = map->elements - map->deleted;
    auto bucket = map->bucket;

    for (; elements > 0; bucket++) {
        if (*bucket && *bucket != &hash_map_deleted) {
            auto key = map->ops->key(*bucket);
            auto pos = (size_t)map->ops->hash(key) & (buckets - 1);
            for (size_t i = 1; ; pos = (pos + i) & (buckets - 1), i++) {
                if (!table[pos]) {
                    table[pos] = *bucket;
                    break;
                }
            }
            elements--;
        }
    }
}

static void hash_map_resize(struct hash_map *map, size_t num)
{
    auto new_buckets = 2 *
        utils_next_power_of_two(map->elements - map->deleted + 1 + num);

    auto new_table = xnew_array(void *, new_buckets);
    hash_map_copy(map, new_table, new_buckets);

    free(map->bucket);
    map->bucket = new_table;
    map->buckets = new_buckets;
    map->elements -= map->deleted;
    map->deleted = 0;
}

void hash_map_reserve(struct hash_map *map, size_t num)
{
    if (map->buckets / 2 - map->elements <= num)
        hash_map_resize(map, num);
}

void hash_map_set(struct hash_map *map, void *val)
{
    hash_map_reserve(map, 1);
    auto key = map->ops->key(val);
    size_t pos = 0;
    auto res = hash_map_search(map, key, &pos);
    if (res == HASH_MAP_BUCKET_EMPTY)
        map->elements++;
    else if (res == HASH_MAP_BUCKET_DELETED)
        map->deleted--;
    map->bucket[pos] = val;
}

u32 hash_str(const char *s)
{
    u32 hash = 7;
    while (*s)
        hash = 31 * hash + (u32)(u8)*s++;
    return hash;
}

u32 hash_mem(void *mem_, size_t len)
{
    u8 *mem = mem_;
    u32 hash = 7;
    for (size_t i = 0; i < len; i++)
        hash = 31 * hash + (u32)mem[i];
    return hash;
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
