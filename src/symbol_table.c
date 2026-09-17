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
    map->capacity = capacity;
    map->buckets = (map_node_t**)calloc(capacity, sizeof(map_node_t*));
    return map;
}

bool map_contains(hash_map_t* map, const char* key) {
    unsigned long index = hash_func(key) % map->capacity;
    map_node_t* current = map->buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->key, key) == 0) return true;
        current = current->next;
    }
    return false;
}

void map_put(hash_map_t* map, const char* key, const char* value) {
    if (map_contains(map, key)) return;

    unsigned long index = hash_func(key) % map->capacity;
    map_node_t* new_node = (map_node_t*)malloc(sizeof(map_node_t));
    
    strncpy(new_node->key, key, 255);
    new_node->key[255] = '\0';
    strncpy(new_node->value, value, 31);
    new_node->value[31] = '\0';
    
    new_node->next = map->buckets[index];
    map->buckets[index] = new_node;
}

void map_free(hash_map_t* map) {
    for (int i = 0; i < map->capacity; i++) {
        map_node_t* current = map->buckets[i];
        while (current != NULL) {
            map_node_t* temp = current;
            current = current->next;
            free(temp);
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