#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdbool.h>

struct scope;

typedef enum {
    SYMBOL_LEXEME,
    SYMBOL_PROGRAM,
    SYMBOL_VARIABLE,
    SYMBOL_PARAMETER,
    SYMBOL_FUNCTION,
    SYMBOL_CLASS,
    SYMBOL_ATTRIBUTE,
    SYMBOL_METHOD
} symbol_usage_t;

typedef struct map_node {
    char* key;
    char* value;
    bool provisional;
    symbol_usage_t usage;
    char *declared_type;
    struct map_node *type_symbol;
    struct scope *declaration_scope;
    struct scope *body_scope;
    int declaration_line;
    struct map_node* next;
} map_node_t;

typedef struct {
    map_node_t** buckets;
    int capacity;
    bool scoped_identifiers;
} hash_map_t;

extern hash_map_t* symbol_table;

extern void init_symbol_table(void);

hash_map_t* map_create(int capacity);
map_node_t *map_get(hash_map_t *map, const char *key, const char *value);
extern bool map_contains(hash_map_t* map, const char* key, const char* value);
extern map_node_t *map_put(hash_map_t* map, const char* key, const char* value);
extern void map_free(hash_map_t* map);
extern void map_print(hash_map_t* map);
void map_purge_provisional(hash_map_t *map);

#endif /* SYMBOL_TABLE_H */
