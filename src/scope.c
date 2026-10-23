#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/scope.h"
#include "../include/diagnostics.h"

struct scope {
    char *path;
    symbol_usage_t kind;
    bool active;
    map_node_t *symbol;
    struct scope *parent;
    struct scope *next_allocated;
};

typedef struct scope_name {
    map_node_t *symbol;
    int line;
    struct scope_name *next;
} scope_name_t;

struct scope_names {
    scope_name_t *first;
    scope_name_t *last;
};

/* Parent links form the active scope stack; frames outlive their symbol references. */
static scope_t *current_scope;
static scope_t *allocated_scopes;
static unsigned int anonymous_scopes;
static map_node_t *inherited_symbol(map_node_t *class_symbol, const char *name, int line);

static void *allocate(size_t size) {
    void *memory = calloc(1, size);
    if (memory == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate scope information\n");
        exit(1);
    }
    return memory;
}

static char *qualified_name(const char *name, const char *scope) {
    char *qualified = allocate(strlen(name) + strlen(scope) + 2);
    sprintf(qualified, "%s:%s", name, scope);
    return qualified;
}

static scope_t *push_scope(const char *name, symbol_usage_t kind) {
    scope_t *frame = allocate(sizeof(*frame));
    if (current_scope == NULL) {
        frame->path = allocate(strlen(name) + 1);
        strcpy(frame->path, name);
    } else {
        frame->path = qualified_name(current_scope->path, name);
    }
    frame->parent = current_scope;
    frame->kind = kind;
    frame->active = true;
    frame->next_allocated = allocated_scopes;
    allocated_scopes = frame;
    current_scope = frame;
    trace_printf("[SCOPE] Enter %s\n", frame->path);
    return frame;
}

void scope_init(void) {
    symbol_table->scoped_identifiers = true;
    anonymous_scopes = 0;
    push_scope("main", SYMBOL_PROGRAM);
}

void scope_destroy(void) {
    while (allocated_scopes != NULL) {
        scope_t *next = allocated_scopes->next_allocated;
        free(allocated_scopes->path);
        free(allocated_scopes);
        allocated_scopes = next;
    }
    current_scope = NULL;
}

static map_node_t *lookup_in(scope_t *scope, const char *name) {
    char *qualified = qualified_name(name, scope->path);
    map_node_t *symbol = map_get(symbol_table, qualified, "ID");
    free(qualified);
    return symbol;
}

static map_node_t *declare(map_node_t *name, symbol_usage_t usage,
                           scope_type_t type, int line) {
    if (name == NULL) return NULL;
    map_node_t *previous = lookup_in(current_scope, name->key);
    if (previous != NULL) {
        report_semantic_error(line,
            "Identifier '%s' is already declared in scope '%s' (line %d)",
            name->key, current_scope->path, previous->declaration_line);
        return NULL;
    }
    char *qualified = qualified_name(name->key, current_scope->path);
    map_node_t *symbol = map_put(symbol_table, qualified, "ID");
    free(qualified);
    symbol->usage = usage;
    symbol->declaration_scope = current_scope;
    symbol->declaration_line = line;
    symbol->owner = current_scope->symbol;
    symbol->type_symbol = type.class_symbol;
    if (type.name != NULL) {
        symbol->declared_type = allocate(strlen(type.name) + 1);
        strcpy(symbol->declared_type, type.name);
    }
    if (symbol->owner != NULL && usage == SYMBOL_ATTRIBUTE) {
        symbol->owner->layout_state = 0;
        symbol_list_append(&symbol->owner->attributes, symbol);
    }
    if (symbol->owner != NULL && usage == SYMBOL_METHOD)
        symbol_list_append(&symbol->owner->methods, symbol);
    if (symbol->owner != NULL && usage == SYMBOL_PARAMETER)
        symbol_list_append(&symbol->owner->parameters, symbol);
    return symbol;
}

void scope_program(map_node_t *name, int line) {
    declare(name, SYMBOL_PROGRAM, (scope_type_t){NULL, NULL}, line);
}

scope_t *scope_begin(map_node_t *name, symbol_usage_t usage, scope_type_t type, int line) {
    map_node_t *symbol = declare(name, usage, type, line);
    char anonymous[48];
    const char *scope_name = name != NULL ? name->key : NULL;
    /* A rejected declaration must not modify the original declaration's scope. */
    if (symbol == NULL) {
        snprintf(anonymous, sizeof(anonymous), "<invalid%u>", ++anonymous_scopes);
        scope_name = anonymous;
    }
    scope_t *frame = push_scope(scope_name, usage);
    frame->symbol = symbol;
    if (symbol != NULL) symbol->body_scope = frame;
    return frame;
}

void scope_end(scope_t *scope) {
    if (scope == NULL || !scope->active) return;
    /* Destructors also restore the stack when Bison abandons a declaration. */
    while (current_scope != NULL) {
        scope_t *closed = current_scope;
        current_scope = closed->parent;
        closed->active = false;
        trace_printf("[SCOPE] Leave %s -> %s\n", closed->path,
               current_scope != NULL ? current_scope->path : "<none>");
        if (closed == scope) break;
    }
}

map_node_t *scope_symbol(const scope_t *scope) {
    return scope != NULL ? scope->symbol : NULL;
}

bool scope_in_function(void) {
    for (scope_t *scope = current_scope; scope != NULL; scope = scope->parent) {
        if (scope->kind == SYMBOL_FUNCTION || scope->kind == SYMBOL_METHOD) return true;
        if (scope->kind == SYMBOL_CLASS) return false;
    }
    return false;
}

map_node_t *scope_resolve(map_node_t *name, scope_reference_t reference, int line) {
    if (name == NULL) return NULL;
    for (scope_t *scope = current_scope; scope != NULL; scope = scope->parent) {
        map_node_t *symbol = lookup_in(scope, name->key);
        if (symbol == NULL && scope->kind == SYMBOL_CLASS && scope->symbol != NULL) {
            int errors = global_errors;
            symbol = inherited_symbol(scope->symbol, name->key, line);
            if (global_errors != errors) return NULL;
        }
        if (symbol == NULL) continue;
        bool valid = reference == SCOPE_VALUE
            ? symbol->usage == SYMBOL_VARIABLE || symbol->usage == SYMBOL_PARAMETER ||
              symbol->usage == SYMBOL_ATTRIBUTE
            : reference == SCOPE_CALLABLE
                ? symbol->usage == SYMBOL_FUNCTION || symbol->usage == SYMBOL_METHOD
                : symbol->usage == SYMBOL_CLASS;
        if (!valid) {
            const char *expected = reference == SCOPE_VALUE ? "a variable" :
                reference == SCOPE_CALLABLE ? "a function or method" : "a class";
            report_semantic_error(line, "Identifier '%s' resolves to '%s', which is not %s",
                                  name->key, symbol->key, expected);
            return NULL;
        }
        trace_printf("[SCOPE] Line %d: %s -> %s\n", line, name->key, symbol->key);
        return symbol;
    }
    report_semantic_error(line, "Identifier '%s' is not declared or not visible in scope '%s'",
                          name->key, current_scope->path);
    return NULL;
}

scope_type_t scope_named_type(map_node_t *name, int line) {
    map_node_t *symbol = scope_resolve(name, SCOPE_CLASS, line);
    return (scope_type_t){symbol != NULL ? symbol->key : name->key, symbol};
}

scope_names_t *scope_names_create(map_node_t *name, int line) {
    scope_names_t *names = allocate(sizeof(*names));
    return scope_names_append(names, name, line);
}

scope_names_t *scope_names_append(scope_names_t *names, map_node_t *name, int line) {
    scope_name_t *entry = allocate(sizeof(*entry));
    entry->symbol = name;
    entry->line = line;
    if (names->last != NULL) names->last->next = entry;
    else names->first = entry;
    names->last = entry;
    return names;
}

void scope_names_free(scope_names_t *names) {
    if (names == NULL) return;
    scope_name_t *entry = names->first;
    while (entry != NULL) {
        scope_name_t *next = entry->next;
        free(entry);
        entry = next;
    }
    free(names);
}

void scope_declare_names(scope_names_t *names, scope_type_t type) {
    symbol_usage_t usage = current_scope->kind == SYMBOL_CLASS
        ? SYMBOL_ATTRIBUTE : SYMBOL_VARIABLE;
    for (scope_name_t *entry = names->first; entry != NULL; entry = entry->next)
        declare(entry->symbol, usage, type, entry->line);
    scope_names_free(names);
}

map_node_t *scope_parameter(map_node_t *name, scope_type_t type, int line) {
    return declare(name, SYMBOL_PARAMETER, type, line);
}

void scope_declare_comptime_names(scope_names_t *names, scope_type_t type) {
    symbol_usage_t usage = current_scope->kind == SYMBOL_CLASS
        ? SYMBOL_ATTRIBUTE : SYMBOL_VARIABLE;
    for (scope_name_t *entry = names->first; entry != NULL; entry = entry->next) {
        map_node_t *symbol = declare(entry->symbol, usage, type, entry->line);
        if (symbol != NULL) symbol->is_comptime = true;
    }
    scope_names_free(names);
}

void scope_class_code(scope_t *scope, map_node_t *code) {
    map_node_t *symbol = scope_symbol(scope);
    if (symbol == NULL || code == NULL) return;
    symbol->class_code = allocate(strlen(code->key) + 1);
    strcpy(symbol->class_code, code->key);
}

void scope_base(map_node_t *name, int line) {
    map_node_t *base = scope_resolve(name, SCOPE_CLASS, line);
    if (current_scope->symbol != NULL) {
        current_scope->symbol->layout_state = 0;
        symbol_list_append(&current_scope->symbol->bases, base);
    }
}

map_node_t *scope_object(map_node_t *name, int line) {
    map_node_t *object = scope_resolve(name, SCOPE_VALUE, line);
    if (object != NULL && object->type_symbol == NULL) {
        report_semantic_error(line, "Identifier '%s' is not an object", name->key);
        return NULL;
    }
    return object;
}

/* TP3 p.11: own declaration first, then inherited candidates, never first-base wins. */
static void find_methods(map_node_t *class_symbol, const char *name,
                         symbol_list_t *path, symbol_list_t *matches, bool *unresolved) {
    for (size_t i = 0; i < path->count; i++) {
        if (path->items[i] == class_symbol) { *unresolved = true; return; }
    }
    symbol_list_append(path, class_symbol);
    map_node_t *own = lookup_in(class_symbol->body_scope, name);
    if (own != NULL) symbol_list_append(matches, own);
    else for (size_t i = 0; i < class_symbol->bases.count; i++)
        find_methods(class_symbol->bases.items[i], name, path, matches, unresolved);
    path->count--;
}

static map_node_t *inherited_symbol(map_node_t *class_symbol, const char *name, int line) {
    symbol_list_t path = {0}, matches = {0};
    bool unresolved = false;
    for (size_t i = 0; i < class_symbol->bases.count; i++)
        find_methods(class_symbol->bases.items[i], name, &path, &matches, &unresolved);
    map_node_t *result = matches.count != 0 ? matches.items[0] : NULL;
    for (size_t i = 1; i < matches.count; i++)
        if (matches.items[i] != result) {
            report_semantic_error(line, "Inherited member '%s' is ambiguous in class '%s'", name, class_symbol->key);
            result = NULL;
            break;
        }
    if (unresolved) {
        report_semantic_error(line, "Cyclic inheritance while resolving '%s'", name);
        result = NULL;
    }
    free(path.items);
    free(matches.items);
    return result;
}

map_node_t *scope_method(map_node_t *object, map_node_t *name, int line) {
    if (object == NULL) return NULL;
    symbol_list_t path = {0}, matches = {0};
    bool unresolved = false;
    find_methods(object->type_symbol, name->key, &path, &matches, &unresolved);
    map_node_t *result = NULL;
    for (size_t i = 0; i < matches.count; i++)
        for (size_t j = i + 1; j < matches.count; ) {
            if (matches.items[i] == matches.items[j])
                matches.items[j] = matches.items[--matches.count];
            else j++;
        }
    if (unresolved)
        report_semantic_error(line, "Cyclic inheritance while resolving method '%s'", name->key);
    else if (matches.count == 0)
        report_semantic_error(line, "Method '%s' does not exist in class '%s' or its bases", name->key, object->type_symbol->key);
    else if (matches.count > 1)
        report_semantic_error(line, "Inherited method '%s' is ambiguous in class '%s'", name->key, object->type_symbol->key);
    else if (matches.items[0]->usage != SYMBOL_METHOD)
        report_semantic_error(line, "Member '%s' is not a method", name->key);
    else result = matches.items[0];
    free(path.items);
    free(matches.items);
    return result;
}

static void append_distinct(symbol_list_t *list, map_node_t *symbol) {
    for (size_t i = 0; i < list->count; i++)
        if (list->items[i] == symbol) return;
    symbol_list_append(list, symbol);
}

bool scope_class_layout(map_node_t *class_symbol, int line) {
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
        if (!scope_class_layout(base, line)) {
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

bool scope_classes_compatible(const map_node_t *left, const map_node_t *right) {
    if (left->effective_attributes.count != right->effective_attributes.count) return false;
    for (size_t i = 0; i < left->effective_attributes.count; i++) {
        const map_node_t *a = left->effective_attributes.items[i];
        const map_node_t *b = right->effective_attributes.items[i];
        if (a->type_symbol != b->type_symbol || a->declared_type == NULL || b->declared_type == NULL ||
            strcmp(a->declared_type, b->declared_type) != 0) return false;
    }
    return true;
}

void scope_class_complete(map_node_t *class_symbol, int line) {
    if (!scope_class_layout(class_symbol, line) || class_symbol->class_code == NULL) return;
    class_symbol->class_complete = true;
    for (int i = 0; i < symbol_table->capacity; i++)
        for (map_node_t *other = symbol_table->buckets[i]; other != NULL; other = other->next)
            if (other != class_symbol && other->usage == SYMBOL_CLASS && other->class_code != NULL &&
                other->class_complete && other->layout_state == 2 && strcmp(other->class_code, class_symbol->class_code) == 0 &&
                !scope_classes_compatible(other, class_symbol))
                report_semantic_error(line, "Classes '%s' and '%s' share code '%s' but have incompatible attribute types or order",
                    class_symbol->key, other->key, class_symbol->class_code);
}

map_node_t *scope_attribute_at(map_node_t *class_symbol, size_t position, int line) {
    if (!scope_class_layout(class_symbol, line)) return NULL;
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
