#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/scope.h"
#include "../include/classes.h"
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

map_node_t *scope_lookup_local(scope_t *scope, const char *name) {
    char *qualified = qualified_name(name, scope->path);
    map_node_t *symbol = map_get(symbol_table, qualified, "ID");
    free(qualified);
    return symbol;
}

static map_node_t *declare(map_node_t *name, symbol_usage_t usage,
                           scope_type_t type, int line) {
    if (name == NULL) return NULL;
    map_node_t *previous = scope_lookup_local(current_scope, name->key);
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
        map_node_t *symbol = scope_lookup_local(scope, name->key);
        if (symbol == NULL && scope->kind == SYMBOL_CLASS && scope->symbol != NULL) {
            class_member_t member = class_inherited_member(scope->symbol, name->key, line);
            if (member.ambiguous || member.cyclic) return NULL;
            symbol = member.symbol;
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

void scope_declare_names(scope_names_t *names, scope_type_t type, bool is_comptime) {
    symbol_usage_t usage = current_scope->kind == SYMBOL_CLASS
        ? SYMBOL_ATTRIBUTE : SYMBOL_VARIABLE;
    for (scope_name_t *entry = names->first; entry != NULL; entry = entry->next) {
        map_node_t *symbol = declare(entry->symbol, usage, type, entry->line);
        if (symbol != NULL) symbol->is_comptime = is_comptime;
    }
    scope_names_free(names);
}

map_node_t *scope_parameter(map_node_t *name, scope_type_t type, bool is_comptime, int line) {
    map_node_t *symbol = declare(name, SYMBOL_PARAMETER, type, line);
    if (symbol != NULL) symbol->is_comptime = is_comptime;
    return symbol;
}

void scope_base(map_node_t *name, int line) {
    map_node_t *base = scope_resolve(name, SCOPE_CLASS, line);
    class_add_base(current_scope->symbol, base);
}

map_node_t *scope_object(map_node_t *name, int line) {
    map_node_t *object = scope_resolve(name, SCOPE_VALUE, line);
    if (object != NULL && object->type_symbol == NULL) {
        report_semantic_error(line, "Identifier '%s' is not an object", name->key);
        return NULL;
    }
    return object;
}
