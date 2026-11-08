#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/symbol_table.h"
#include "../include/memory.h"

hash_map_t* symbol_table = NULL;

void symbol_list_append(symbol_list_t *list, map_node_t *symbol) {
    if (symbol == NULL) return;
    map_node_t **items = realloc(list->items, (list->count + 1) * sizeof(*items));
    if (items == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate symbol metadata\n");
        exit(1);
    }
    list->items = items;
    list->items[list->count++] = symbol;
}

void symbol_set_usage(map_node_t *symbol, symbol_usage_t usage) {
    assert(symbol != NULL);
    assert(symbol->usage == SYMBOL_LEXEME || symbol->usage == usage);
    if (usage == SYMBOL_CLASS) {
        assert(symbol->function_info == NULL);
        if (symbol->class_info == NULL)
            symbol->class_info = memory_zero(1, sizeof(*symbol->class_info));
    } else if (usage == SYMBOL_FUNCTION || usage == SYMBOL_METHOD) {
        assert(symbol->class_info == NULL);
        if (symbol->function_info == NULL)
            symbol->function_info = memory_zero(1, sizeof(*symbol->function_info));
    }
    symbol->usage = usage;
}

symbol_capture_info_t *symbol_ensure_captures(map_node_t *symbol) {
    assert(symbol != NULL);
    assert(symbol->usage == SYMBOL_FUNCTION || symbol->usage == SYMBOL_METHOD ||
           symbol->usage == SYMBOL_OBJECT_VALUE);
    if (symbol->capture_info == NULL)
        symbol->capture_info = memory_zero(1, sizeof(*symbol->capture_info));
    return symbol->capture_info;
}

void symbol_clear(map_node_t *symbol) {
    if (symbol == NULL) return;
    free(symbol->key);
    free(symbol->value);
    free(symbol->declared_type);
    if (symbol->function_info != NULL) {
        free(symbol->function_info->parameters.items);
        free(symbol->function_info);
    }
    if (symbol->class_info != NULL) {
        free(symbol->class_info->class_code);
        free(symbol->class_info->attributes.items);
        free(symbol->class_info->methods.items);
        free(symbol->class_info->bases.items);
        free(symbol->class_info->effective_attributes.items);
        free(symbol->class_info);
    }
    if (symbol->capture_info != NULL) {
        free(symbol->capture_info->comptime_bindings.items);
        free(symbol->capture_info->comptime_values.items);
        free(symbol->capture_info->object_fields.items);
        free(symbol->capture_info->object_values.items);
        free(symbol->capture_info);
    }
    *symbol = (map_node_t){0};
}

static void free_symbol(map_node_t *symbol) {
    symbol_clear(symbol);
    free(symbol);
}

static void print_list(const char *label, const symbol_list_t *list) {
    printf(" | %s: [", label);
    for (size_t i = 0; i < list->count; i++) {
        if (i != 0) printf(", ");
        printf("%s", list->items[i]->key);
    }
    printf("]");
}

// djb2 hash function
static unsigned long hash_func(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; 
    }
    return hash;
}

hash_map_t* map_create(int capacity) {
    hash_map_t* map = (hash_map_t*)malloc(sizeof(hash_map_t));
    if (map == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate symbol table\n");
        exit(1);
    }
    map->capacity = capacity;
    map->scoped_identifiers = false;
    map->buckets = (map_node_t**)calloc(capacity, sizeof(map_node_t*));
    if (map->buckets == NULL) {
        free(map);
        fprintf(stderr, "Fatal error: Cannot allocate symbol table buckets\n");
        exit(1);
    }
    return map;
}

map_node_t *map_get(hash_map_t* map, const char* key, const char* value) {
    unsigned long index = hash_func(key) % map->capacity;
    map_node_t* current = map->buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->key, key) == 0 && strcmp(current->value, value) == 0) return current;
        current = current->next;
    }
    return NULL;
}

map_node_t *map_put(hash_map_t* map, const char* key, const char* value) {
    unsigned long index = hash_func(key) % map->capacity;
    for (map_node_t *current = map->buckets[index]; current != NULL; current = current->next) {
        if (strcmp(current->key, key) == 0 && strcmp(current->value, value) == 0) {
            return current;
        }
    }

    map_node_t* new_node = (map_node_t*)calloc(1, sizeof(map_node_t));
    if (new_node == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate symbol\n");
        exit(1);
    }
    new_node->key = strdup(key);
    new_node->value = strdup(value);
    if (new_node->key == NULL || new_node->value == NULL) {
        free(new_node->key);
        free(new_node->value);
        free(new_node);
        fprintf(stderr, "Fatal error: Cannot allocate symbol text\n");
        exit(1);
    }
    
    new_node->provisional = false;
    new_node->next = map->buckets[index];
    map->buckets[index] = new_node;
    return new_node;
}

map_node_t *add_to_symbol_table(const char *lexeme, const char *category) {
    return map_put(symbol_table, lexeme, category);
}

void map_free(hash_map_t* map) {
    for (int i = 0; i < map->capacity; i++) {
        map_node_t* current = map->buckets[i];
        while (current != NULL) {
            map_node_t* temp = current;
            current = current->next;
            free_symbol(temp);
        }
    }
    free(map->buckets);
    free(map);
}

void map_print(hash_map_t* map) {
    printf("\n--- SYMBOL TABLE ---\n");
    for (int i = 0; i < map->capacity; i++) {
        map_node_t* current = map->buckets[i];
        while (current != NULL) {
            if (!current->provisional &&
                !(map->scoped_identifiers && strcmp(current->value, "ID") == 0 &&
                  current->usage == SYMBOL_LEXEME)) {
                static const char *usage_names[] = {
                    "LEXEME", "PROGRAM", "VARIABLE", "PARAMETER", "FUNCTION",
                    "CLASS", "ATTRIBUTE", "METHOD", "OBJECT_VALUE", "AUXILIARY"
                };
                printf("Lexeme: %s | Type: %s", current->key, current->value);
                if (current->usage != SYMBOL_LEXEME) {
                    const symbol_class_info_t *class_info = current->class_info;
                    const symbol_function_info_t *function_info = current->function_info;
                    const symbol_capture_info_t *capture_info = current->capture_info;
                    printf(" | Use: %s", usage_names[current->usage]);
                    if (current->declared_type != NULL)
                        printf(" | Declared type: %s", current->declared_type);
                    printf(" | Line: %d", current->declaration_line);
                    if (current->is_comptime) printf(" | Comptime: yes");
                    if (current->comptime_value != NULL)
                        printf(" | Compile-time value: %s", current->comptime_value->key);
                    if (function_info != NULL && function_info->source_function != NULL)
                        printf(" | Source function: %s", function_info->source_function->key);
                    if (function_info != NULL && function_info->comptime_template)
                        printf(" | Compile-time template: yes");
                    if (capture_info != NULL && capture_info->comptime_bindings.count != 0) {
                        printf(" | Compile-time bindings: [");
                        for (size_t j = 0; j < capture_info->comptime_bindings.count; j++) {
                            if (j != 0) printf(", ");
                            printf("%s=%s", capture_info->comptime_bindings.items[j]->key,
                                capture_info->comptime_values.items[j]->key);
                        }
                        printf("]");
                    }
                    if (capture_info != NULL && capture_info->object_fields.count != 0) {
                        printf(" | Object fields: [");
                        for (size_t j = 0; j < capture_info->object_fields.count; j++) {
                            if (j != 0) printf(", ");
                            printf("%s=%s", capture_info->object_fields.items[j]->key,
                                capture_info->object_values.items[j]->key);
                        }
                        printf("]");
                    }
                    if (class_info != NULL && class_info->class_code != NULL)
                        printf(" | Class code: %s", class_info->class_code);
                    if (current->usage == SYMBOL_FUNCTION || current->usage == SYMBOL_METHOD)
                        print_list("Parameters", &function_info->parameters);
                    if (current->usage == SYMBOL_CLASS) {
                        print_list("Attributes", &class_info->attributes);
                        print_list("Effective attributes", &class_info->effective_attributes);
                        print_list("Methods", &class_info->methods);
                        print_list("Bases", &class_info->bases);
                    }
                }
                putchar('\n');
            }
            current = current->next;
        }
    }
    printf("-------------------------\n");
}

void print_symbol_table(void) {
    if (symbol_table != NULL) {
        map_print(symbol_table);
    }
}

void init_symbol_table(void) {
    if (symbol_table == NULL) {
        symbol_table = map_create(256); 
    }
}

/* Call only after yyparse returns: parser values borrow these nodes. */
void map_purge_provisional(hash_map_t *map) {
    for (int i = 0; i < map->capacity; i++) {
        map_node_t **link = &map->buckets[i];
        while (*link != NULL) {
            map_node_t *node = *link;
            if (node->provisional) {
                *link = node->next;
                free_symbol(node);
            } else {
                link = &node->next;
            }
        }
    }
}
