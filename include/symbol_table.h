#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdbool.h>

typedef struct map_node {
    char key[256];
    char value[32];
    struct map_node* next;
} map_node_t;

typedef struct {
    map_node_t** buckets;
    int capacity;
} hash_map_t;

extern hash_map_t* symbol_table;

extern void init_symbol_table(void);

hash_map_t* map_create(int capacity);
extern bool map_contains(hash_map_t* map, const char* key);
extern void map_put(hash_map_t* map, const char* key, const char* value);
extern void map_free(hash_map_t* map);
extern void map_print(hash_map_t* map);

#endif /* SYMBOL_TABLE_H */

