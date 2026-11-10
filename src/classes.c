#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/classes.h"
#include "../include/scope.h"
#include "../include/diagnostics.h"
#include "../include/memory.h"

typedef struct {
    map_node_t *type;
    size_t next;
} class_frame_t;

typedef struct {
    class_frame_t *items;
    size_t count, capacity;
} class_stack_t;

static void class_stack_push(class_stack_t *stack, map_node_t *type) {
    if (stack->count == stack->capacity) {
        stack->capacity = stack->capacity ? memory_multiply(stack->capacity, 2) : 16;
        stack->items = memory_resize(stack->items, memory_multiply(stack->capacity, sizeof(*stack->items)));
    }
    stack->items[stack->count++] = (class_frame_t){type, 0};
}

void class_set_code(map_node_t *symbol, map_node_t *code) {
    if (symbol == NULL || code == NULL) return;
    symbol->class_info->class_code = strdup(code->key);
    if (symbol->class_info->class_code == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate class information\n");
        exit(1);
    }
}

void class_add_base(map_node_t *class_symbol, map_node_t *base) {
    if (class_symbol == NULL) return;
    class_symbol->class_info->layout_state = 0;
    symbol_list_append(&class_symbol->class_info->bases, base);
}

static void append_distinct(symbol_list_t *list, map_node_t *symbol) {
    for (size_t i = 0; i < list->count; i++)
        if (list->items[i] == symbol) return;
    symbol_list_append(list, symbol);
}

/* TP3 p.11: own declaration first, then inherited candidates, never first-base wins. */
static void find_members(map_node_t *class_symbol, const char *name,
                         symbol_list_t *matches, bool *unresolved) {
    class_stack_t stack = {0};
    class_stack_push(&stack, class_symbol);
    while (stack.count) {
        class_frame_t *frame = &stack.items[stack.count - 1];
        map_node_t *type = frame->type;
        if (!frame->next) {
            map_node_t *own = scope_lookup_local(type->body_scope, name);
            if (own) {
                append_distinct(matches, own);
                stack.count--;
                continue;
            }
        }
        if (frame->next == type->class_info->bases.count) {
            stack.count--;
            continue;
        }
        map_node_t *base = type->class_info->bases.items[frame->next++];
        bool cycle = false;
        for (size_t i = 0; i < stack.count; i++)
            if (stack.items[i].type == base) cycle = true;
        if (cycle) *unresolved = true;
        else class_stack_push(&stack, base);
    }
    free(stack.items);
}

class_member_t class_lookup_member(map_node_t *class_symbol, const char *name,
                                  class_search_t search) {
    symbol_list_t matches = {0};
    bool cyclic = false;
    if (search == CLASS_INHERITED_MEMBERS) {
        for (size_t i = 0; i < class_symbol->class_info->bases.count; i++)
            find_members(class_symbol->class_info->bases.items[i], name, &matches, &cyclic);
    } else find_members(class_symbol, name, &matches, &cyclic);
    class_member_t result = {
        .symbol = matches.count != 0 ? matches.items[0] : NULL,
        .ambiguous = matches.count > 1,
        .cyclic = cyclic
    };
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
    if (class_symbol->class_info->layout_state == 2) return true;
    if (class_symbol->class_info->layout_state == 3) return false;
    class_stack_t stack = {0};
    class_stack_push(&stack, class_symbol);
    class_symbol->class_info->layout_state = 1;
    class_symbol->class_info->effective_attributes.count = 0;
    while (stack.count) {
        class_frame_t *frame = &stack.items[stack.count - 1];
        symbol_class_info_t *info = frame->type->class_info;
        if (frame->next < info->bases.count) {
            map_node_t *base = info->bases.items[frame->next];
            if (base->class_info->layout_state == 1) {
                report_semantic_error(line, "Cyclic inheritance in class '%s'", base->key);
                break;
            }
            if (base->class_info->layout_state == 3) break;
            if (base->class_info->layout_state == 0) {
                base->class_info->layout_state = 1;
                base->class_info->effective_attributes.count = 0;
                class_stack_push(&stack, base);
                continue;
            }
            for (size_t j = 0; j < base->class_info->effective_attributes.count; j++)
                append_distinct(&info->effective_attributes, base->class_info->effective_attributes.items[j]);
            frame->next++;
        } else {
            for (size_t i = 0; i < info->attributes.count; i++)
                append_distinct(&info->effective_attributes, info->attributes.items[i]);
            info->layout_state = 2;
            stack.count--;
        }
    }
    bool valid = stack.count == 0;
    while (stack.count) stack.items[--stack.count].type->class_info->layout_state = 3;
    free(stack.items);
    return valid;
}

bool class_same_layout(const map_node_t *left, const map_node_t *right) {
    if (left->class_info->effective_attributes.count != right->class_info->effective_attributes.count) return false;
    for (size_t i = 0; i < left->class_info->effective_attributes.count; i++) {
        const map_node_t *a = left->class_info->effective_attributes.items[i];
        const map_node_t *b = right->class_info->effective_attributes.items[i];
        if (a->type_symbol != b->type_symbol || a->declared_type == NULL || b->declared_type == NULL ||
            strcmp(a->declared_type, b->declared_type) != 0) return false;
    }
    return true;
}

void class_complete(map_node_t *class_symbol, int line) {
    if (!class_build_layout(class_symbol, line) || class_symbol->class_info->class_code == NULL) return;
    class_symbol->class_info->class_complete = true;
    for (int i = 0; i < symbol_table->capacity; i++)
        for (map_node_t *other = symbol_table->buckets[i]; other != NULL; other = other->next)
            if (other != class_symbol && other->usage == SYMBOL_CLASS && other->class_info->class_code != NULL &&
                other->class_info->class_complete && other->class_info->layout_state == 2 && strcmp(other->class_info->class_code, class_symbol->class_info->class_code) == 0 &&
                !class_same_layout(other, class_symbol))
                report_semantic_error(line, "Classes '%s' and '%s' share code '%s' but have incompatible attribute types or order",
                    class_symbol->key, other->key, class_symbol->class_info->class_code);
}

map_node_t *class_attribute_at(map_node_t *class_symbol, size_t position, int line) {
    if (!class_build_layout(class_symbol, line)) return NULL;
    if (position >= class_symbol->class_info->effective_attributes.count) {
        report_semantic_error(line, "Attribute position %zu does not exist in class '%s'", position, class_symbol->key);
        return NULL;
    }
    map_node_t *attribute = class_symbol->class_info->effective_attributes.items[position];
    const char *separator = strchr(attribute->key, ':');
    size_t length = (size_t)(separator - attribute->key);
    for (size_t i = 0; i < class_symbol->class_info->attributes.count; i++) {
        map_node_t *own = class_symbol->class_info->attributes.items[i];
        if (strncmp(own->key, attribute->key, length) == 0 && own->key[length] == ':') return attribute;
    }
    for (size_t i = 0; i < class_symbol->class_info->effective_attributes.count; i++) {
        map_node_t *other = class_symbol->class_info->effective_attributes.items[i];
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
        destination->class_info == NULL || source->class_info == NULL ||
        destination->class_info->class_code == NULL || source->class_info->class_code == NULL) return true;
    if (strcmp(destination->class_info->class_code, source->class_info->class_code) == 0) return true;
    report_semantic_error(line, "Object assignment requires matching class codes (topic 24)");
    return false;
}

bool class_check_containment(void) {
    class_stack_t stack = {0};
    bool valid = true;
    for (int i = 0; i < symbol_table->capacity && valid; i++)
        for (map_node_t *symbol = symbol_table->buckets[i]; symbol && valid; symbol = symbol->next) {
            if (symbol->usage != SYMBOL_CLASS || symbol->class_info->containment_state == 2) continue;
            symbol->class_info->containment_state = 1;
            class_stack_push(&stack, symbol);
            while (stack.count) {
                class_frame_t *frame = &stack.items[stack.count - 1];
                symbol_class_info_t *info = frame->type->class_info;
                if (frame->next == info->effective_attributes.count) {
                    info->containment_state = 2;
                    stack.count--;
                    continue;
                }
                map_node_t *field = info->effective_attributes.items[frame->next];
                map_node_t *child = field->type_symbol;
                if (child && child->class_info->containment_state == 1) {
                    report_semantic_error(child->declaration_line,
                        "Recursive object containment in class '%s'", child->key);
                    valid = false;
                    break;
                }
                if (child && child->class_info->containment_state == 0) {
                    child->class_info->containment_state = 1;
                    class_stack_push(&stack, child);
                    continue;
                }
                info->has_comptime |= field->is_comptime || (child && child->class_info->has_comptime);
                frame->next++;
            }
        }
    while (stack.count) stack.items[--stack.count].type->class_info->containment_state = 3;
    free(stack.items);
    return valid;
}

bool class_has_comptime(const map_node_t *class_symbol) {
    /* Computed for the immutable class graph by class_check_containment, before specialization. */
    return class_symbol && class_symbol->class_info->has_comptime;
}
