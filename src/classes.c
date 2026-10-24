#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/classes.h"
#include "../include/scope.h"
#include "../include/diagnostics.h"

void class_set_code(map_node_t *symbol, map_node_t *code) {
    if (symbol == NULL || code == NULL) return;
    symbol->class_code = strdup(code->key);
    if (symbol->class_code == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate class information\n");
        exit(1);
    }
}

void class_add_base(map_node_t *class_symbol, map_node_t *base) {
    if (class_symbol == NULL) return;
    class_symbol->layout_state = 0;
    symbol_list_append(&class_symbol->bases, base);
}

static void append_distinct(symbol_list_t *list, map_node_t *symbol) {
    for (size_t i = 0; i < list->count; i++)
        if (list->items[i] == symbol) return;
    symbol_list_append(list, symbol);
}

/* TP3 p.11: own declaration first, then inherited candidates, never first-base wins. */
static void find_members(map_node_t *class_symbol, const char *name,
                         symbol_list_t *path, symbol_list_t *matches, bool *unresolved) {
    for (size_t i = 0; i < path->count; i++) {
        if (path->items[i] == class_symbol) { *unresolved = true; return; }
    }
    symbol_list_append(path, class_symbol);
    map_node_t *own = scope_lookup_local(class_symbol->body_scope, name);
    if (own != NULL) append_distinct(matches, own);
    else for (size_t i = 0; i < class_symbol->bases.count; i++)
        find_members(class_symbol->bases.items[i], name, path, matches, unresolved);
    path->count--;
}

class_member_t class_lookup_member(map_node_t *class_symbol, const char *name,
                                  class_search_t search) {
    symbol_list_t path = {0}, matches = {0};
    bool cyclic = false;
    if (search == CLASS_INHERITED_MEMBERS) {
        for (size_t i = 0; i < class_symbol->bases.count; i++)
            find_members(class_symbol->bases.items[i], name, &path, &matches, &cyclic);
    } else find_members(class_symbol, name, &path, &matches, &cyclic);
    class_member_t result = {
        .symbol = matches.count != 0 ? matches.items[0] : NULL,
        .ambiguous = matches.count > 1,
        .cyclic = cyclic
    };
    free(path.items);
    free(matches.items);
    return result;
}

class_member_t class_inherited_member(map_node_t *class_symbol, const char *name, int line) {
    class_member_t result = class_lookup_member(class_symbol, name, CLASS_INHERITED_MEMBERS);
    if (result.ambiguous)
        report_semantic_error(line, "Inherited member '%s' is ambiguous in class '%s'", name, class_symbol->key);
    if (result.cyclic)
        report_semantic_error(line, "Cyclic inheritance while resolving '%s'", name);
    return result;
}

map_node_t *class_resolve_method(map_node_t *object, map_node_t *name, int line) {
    if (object == NULL) return NULL;
    class_member_t member = class_lookup_member(object->type_symbol, name->key, CLASS_ALL_MEMBERS);
    if (member.cyclic)
        report_semantic_error(line, "Cyclic inheritance while resolving method '%s'", name->key);
    else if (member.symbol == NULL)
        report_semantic_error(line, "Method '%s' does not exist in class '%s' or its bases", name->key, object->type_symbol->key);
    else if (member.ambiguous)
        report_semantic_error(line, "Inherited method '%s' is ambiguous in class '%s'", name->key, object->type_symbol->key);
    else if (member.symbol->usage != SYMBOL_METHOD)
        report_semantic_error(line, "Member '%s' is not a method", name->key);
    else return member.symbol;
    return NULL;
}

bool class_build_layout(map_node_t *class_symbol, int line) {
    if (class_symbol == NULL) return false;
    if (class_symbol->layout_state == 2) return true;
    if (class_symbol->layout_state == 3) return false;
    if (class_symbol->layout_state == 1) {
        report_semantic_error(line, "Cyclic inheritance in class '%s'", class_symbol->key);
        class_symbol->layout_state = 3;
        return false;
    }
    class_symbol->layout_state = 1;
    class_symbol->effective_attributes.count = 0;
    for (size_t i = 0; i < class_symbol->bases.count; i++) {
        map_node_t *base = class_symbol->bases.items[i];
        if (!class_build_layout(base, line)) {
            class_symbol->layout_state = 3;
            return false;
        }
        for (size_t j = 0; j < base->effective_attributes.count; j++)
            append_distinct(&class_symbol->effective_attributes, base->effective_attributes.items[j]);
    }
    for (size_t i = 0; i < class_symbol->attributes.count; i++)
        append_distinct(&class_symbol->effective_attributes, class_symbol->attributes.items[i]);
    class_symbol->layout_state = 2;
    return true;
}

bool class_same_layout(const map_node_t *left, const map_node_t *right) {
    if (left->effective_attributes.count != right->effective_attributes.count) return false;
    for (size_t i = 0; i < left->effective_attributes.count; i++) {
        const map_node_t *a = left->effective_attributes.items[i];
        const map_node_t *b = right->effective_attributes.items[i];
        if (a->type_symbol != b->type_symbol || a->declared_type == NULL || b->declared_type == NULL ||
            strcmp(a->declared_type, b->declared_type) != 0) return false;
    }
    return true;
}

void class_complete(map_node_t *class_symbol, int line) {
    if (!class_build_layout(class_symbol, line) || class_symbol->class_code == NULL) return;
    class_symbol->class_complete = true;
    for (int i = 0; i < symbol_table->capacity; i++)
        for (map_node_t *other = symbol_table->buckets[i]; other != NULL; other = other->next)
            if (other != class_symbol && other->usage == SYMBOL_CLASS && other->class_code != NULL &&
                other->class_complete && other->layout_state == 2 && strcmp(other->class_code, class_symbol->class_code) == 0 &&
                !class_same_layout(other, class_symbol))
                report_semantic_error(line, "Classes '%s' and '%s' share code '%s' but have incompatible attribute types or order",
                    class_symbol->key, other->key, class_symbol->class_code);
}

map_node_t *class_attribute_at(map_node_t *class_symbol, size_t position, int line) {
    if (!class_build_layout(class_symbol, line)) return NULL;
    if (position >= class_symbol->effective_attributes.count) {
        report_semantic_error(line, "Attribute position %zu does not exist in class '%s'", position, class_symbol->key);
        return NULL;
    }
    map_node_t *attribute = class_symbol->effective_attributes.items[position];
    const char *separator = strchr(attribute->key, ':');
    size_t length = (size_t)(separator - attribute->key);
    for (size_t i = 0; i < class_symbol->attributes.count; i++) {
        map_node_t *own = class_symbol->attributes.items[i];
        if (strncmp(own->key, attribute->key, length) == 0 && own->key[length] == ':') return attribute;
    }
    for (size_t i = 0; i < class_symbol->effective_attributes.count; i++) {
        map_node_t *other = class_symbol->effective_attributes.items[i];
        if (other != attribute && strncmp(other->key, attribute->key, length) == 0 && other->key[length] == ':') {
            report_semantic_error(line, "Inherited attribute '%.*s' is ambiguous in class '%s'",
                (int)length, attribute->key, class_symbol->key);
            return NULL;
        }
    }
    return attribute;
}

bool class_check_copy(const map_node_t *destination, const map_node_t *source, int line) {
    if (destination == NULL || source == NULL || destination == source ||
        destination->class_code == NULL || source->class_code == NULL) return true;
    if (strcmp(destination->class_code, source->class_code) == 0) return true;
    report_semantic_error(line, "Object assignment requires matching class codes (topic 24)");
    return false;
}

typedef struct type_path {
    const map_node_t *type;
    const struct type_path *parent;
} type_path_t;


static bool type_in_path(const type_path_t *path, const map_node_t *type) {
    for (; path != NULL; path = path->parent)
        if (path->type == type) return true;
    return false;
}

static bool class_has_comptime_path(const map_node_t *class_symbol, const type_path_t *path) {
    if (class_symbol == NULL || type_in_path(path, class_symbol)) return false;
    type_path_t current = {class_symbol, path};
    for (size_t i = 0; i < class_symbol->effective_attributes.count; i++) {
        const map_node_t *field = class_symbol->effective_attributes.items[i];
        if (field->is_comptime || class_has_comptime_path(field->type_symbol, &current)) return true;
    }
    return false;
}

bool class_has_comptime(const map_node_t *class_symbol) {
    return class_has_comptime_path(class_symbol, NULL);
}
