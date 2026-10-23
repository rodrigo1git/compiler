#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdbool.h>
#include <stddef.h>

struct scope;
struct map_node;
typedef struct {
    struct map_node **items;
    size_t count;
} symbol_list_t;

typedef enum {
    SYMBOL_LEXEME,
    SYMBOL_PROGRAM,
    SYMBOL_VARIABLE,
    SYMBOL_PARAMETER,
    SYMBOL_FUNCTION,
    SYMBOL_CLASS,
    SYMBOL_ATTRIBUTE,
    SYMBOL_METHOD,
    SYMBOL_OBJECT_VALUE
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
    struct map_node *owner;
    symbol_list_t parameters;
    symbol_list_t attributes;
    symbol_list_t methods;
    symbol_list_t bases;
    symbol_list_t effective_attributes;
    unsigned int layout_state;
    bool class_complete;
    char *class_code;
    bool is_comptime;
    struct map_node *comptime_value;
    struct map_node *source_function;
    symbol_list_t comptime_bindings;
    symbol_list_t comptime_values;
    symbol_list_t object_fields;
    symbol_list_t object_values;
    bool comptime_template;
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
void symbol_list_append(symbol_list_t *list, map_node_t *symbol);

#endif /* SYMBOL_TABLE_H */
