#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/symbol_table.h"

hash_map_t* symbol_table = NULL;

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
    if (map == NULL) return NULL;
    map->capacity = capacity;
    map->buckets = (map_node_t**)calloc(capacity, sizeof(map_node_t*));
    map->retired_nodes = NULL;
    if (map->buckets == NULL) {
        free(map);
        return NULL;
    }
    return map;
}

static map_node_t *create_node(const char* key, const char* value) {
    map_node_t* node = (map_node_t*)malloc(sizeof(map_node_t));
    if (node == NULL) return NULL;
    node->key = strdup(key);
    node->value = strdup(value);
    if (node->key == NULL || node->value == NULL) {
        free(node->key);
        free(node->value);
        free(node);
        return NULL;
    }
    node->next = NULL;
    return node;
}

bool map_contains(hash_map_t* map, const char* key, const char* value) {
    unsigned long index = hash_func(key) % map->capacity;
    map_node_t* current = map->buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->key, key) == 0 && strcmp(current->value, value) == 0) return true;
        current = current->next;
    }
    return false;
}

map_node_t *map_put(hash_map_t* map, const char* key, const char* value) {
    if (map == NULL || key == NULL || value == NULL || map->capacity <= 0) return NULL;
    unsigned long index = hash_func(key) % map->capacity;
    for (map_node_t *current = map->buckets[index]; current != NULL; current = current->next) {
        if (strcmp(current->key, key) == 0 && strcmp(current->value, value) == 0) {
            return current;
        }
    }

    map_node_t* new_node = create_node(key, value);
    if (new_node == NULL) return NULL;
    new_node->next = map->buckets[index];
    map->buckets[index] = new_node;
    return new_node;
}

map_node_t *map_create_detached(hash_map_t* map, const char* key, const char* value) {
    if (map == NULL || key == NULL || value == NULL) return NULL;
    map_node_t* node = create_node(key, value);
    if (node == NULL) return NULL;
    node->next = map->retired_nodes;
    map->retired_nodes = node;
    return node;
}

bool map_remove(hash_map_t* map, const char* key, const char* value) {
    if (map == NULL || key == NULL || value == NULL || map->capacity <= 0) return false;
    unsigned long index = hash_func(key) % map->capacity;
    map_node_t **link = &map->buckets[index];

    while (*link != NULL) {
        map_node_t *node = *link;
        if (strcmp(node->key, key) == 0 && strcmp(node->value, value) == 0) {
            *link = node->next;
            node->next = map->retired_nodes;
            map->retired_nodes = node;
            return true;
        }
        link = &node->next;
    }

    return false;
}

void map_free(hash_map_t* map) {
    if (map == NULL) return;
    for (int i = 0; i < map->capacity; i++) {
        map_node_t* current = map->buckets[i];
        while (current != NULL) {
            map_node_t* temp = current;
            current = current->next;
            free(temp->key);
            free(temp->value);
            free(temp);
        }
    }
    map_node_t* retired = map->retired_nodes;
    while (retired != NULL) {
        map_node_t* next = retired->next;
        free(retired->key);
        free(retired->value);
        free(retired);
        retired = next;
    }
    free(map->buckets);
    free(map);
}

void map_print(hash_map_t* map) {
    printf("\n--- SYMBOL TABLE ---\n");
    for (int i = 0; i < map->capacity; i++) {
        map_node_t* current = map->buckets[i];
        while (current != NULL) {
            printf("Lexeme: %s | Type: %s\n", current->key, current->value);
            current = current->next;
        }
    }
    printf("-------------------------\n");
}

void init_symbol_table(void) {
    if (symbol_table == NULL) {
        symbol_table = map_create(256); 
    }
}
