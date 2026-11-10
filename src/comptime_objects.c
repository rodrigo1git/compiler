#include "../include/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/comptime_objects.h"
#include "../include/classes.h"
#include "../include/diagnostics.h"

typedef struct object_location {
    const map_node_t *parent;
    const map_node_t *field;
    map_node_t symbol;
    struct object_location *next;
} object_location_t;

static object_location_t *object_locations;

const map_node_t *comptime_object_location(const map_node_t *parent, const map_node_t *field) {
    if (parent == NULL || field == NULL) return NULL;
    for (object_location_t *item = object_locations; item != NULL; item = item->next)
        if (item->parent == parent && item->field == field) return &item->symbol;
    object_location_t *item = memory_resize(NULL, sizeof(*item));
    *item = (object_location_t){.parent = parent, .field = field, .next = object_locations};
    size_t length = memory_add(memory_add(strlen(parent->key), strlen(field->key)), 3);
    item->symbol.key = memory_resize(NULL, length);
    snprintf(item->symbol.key, length, "%s[%s]", parent->key, field->key);
    item->symbol.owner = parent->owner;
    item->symbol.type_symbol = field->type_symbol;
    object_locations = item;
    return &item->symbol;
}

const map_node_t *comptime_object_root(const map_node_t *object) {
    for (;;) {
        object_location_t *item = object_locations;
        while (item != NULL && &item->symbol != object) item = item->next;
        if (item == NULL) return object;
        object = item->parent;
    }
}

const map_node_t *comptime_resolve_object(const map_node_t *object, const map_node_t *receiver) {
    if (object == NULL) return receiver;
    return object->usage == SYMBOL_ATTRIBUTE ? comptime_object_location(receiver, object) : object;
}

comptime_value_t comptime_object_field(comptime_value_t object, const map_node_t *field,
                                       const comptime_environment_t *environment) {
    comptime_value_t result = {0};
    const map_node_t *snapshot = object.object_snapshot;
    if (snapshot != NULL) {
        const symbol_capture_info_t *capture = snapshot->capture_info;
        if (capture != NULL) {
            for (size_t i = 0; i < capture->comptime_bindings.count; i++)
                if (capture->comptime_bindings.items[i] == field)
                    return comptime_read_constant(capture->comptime_values.items[i]);
            for (size_t i = 0; i < capture->object_fields.count; i++)
                if (capture->object_fields.items[i] == field)
                    result.object_snapshot = capture->object_values.items[i];
        }
        if (field->is_comptime) return result;
        if (field->type_symbol == NULL)
            result = comptime_get(environment, field, object.object);
    } else result = comptime_get(environment, field, object.object);
    if (field->type_symbol != NULL) {
        result.object_type = field->type_symbol;
        result.object = comptime_object_location(object.object, field);
    }
    return result;
}

/* Grow traversal storage geometrically; all object walks use heap-backed frames. */
static void *grow_object_stack(void *items, size_t *capacity, size_t needed, size_t item_size) {
    if (needed <= *capacity) return items;
    *capacity = *capacity ? memory_multiply(*capacity, 2) : 16;
    if (*capacity < needed) *capacity = needed;
    return memory_resize(items, memory_multiply(*capacity, item_size));
}

static map_node_t *capture_snapshot(comptime_value_t value, unsigned int *serial, int line) {
    char name[64];
    snprintf(name, sizeof(name), "@value%u", ++(*serial));
    map_node_t *snapshot = map_put(symbol_table, name, "ID");
    symbol_set_usage(snapshot, SYMBOL_OBJECT_VALUE);
    snapshot->type_symbol = (map_node_t *)value.object_type;
    snapshot->declared_type = memory_resize(NULL, memory_add(strlen(value.object_type->key), 1));
    strcpy(snapshot->declared_type, value.object_type->key);
    snapshot->declaration_line = line;
    return snapshot;
}

typedef struct {
    comptime_value_t value;
    map_node_t *snapshot;
    size_t next;
} capture_frame_t;

map_node_t *comptime_capture_object(comptime_value_t value, const comptime_environment_t *environment,
                                    unsigned int *serial, int line) {
    /* Source type checking has already rejected cyclic containment. */
    if (!class_has_comptime(value.object_type)) return NULL;
    map_node_t *root = capture_snapshot(value, serial, line);
    size_t count = 1, capacity = 0;
    capture_frame_t *stack = grow_object_stack(NULL, &capacity, count, sizeof(*stack));
    stack[0] = (capture_frame_t){value, root, 0};
    while (count) {
        capture_frame_t *frame = &stack[count - 1];
        const symbol_list_t *fields = &frame->value.object_type->class_info->effective_attributes;
        if (frame->next == fields->count) { count--; continue; }
        map_node_t *field = fields->items[frame->next++];
        if (!field->is_comptime && !class_has_comptime(field->type_symbol)) continue;
        comptime_value_t field_value = comptime_object_field(frame->value, field, environment);
        if (field->type_symbol) {
            if (!class_has_comptime(field_value.object_type)) continue;
            map_node_t *child = capture_snapshot(field_value, serial, line);
            symbol_capture_info_t *capture = symbol_ensure_captures(frame->snapshot);
            symbol_list_append(&capture->object_fields, field);
            symbol_list_append(&capture->object_values, child);
            stack = grow_object_stack(stack, &capacity, memory_add(count, 1), sizeof(*stack));
            stack[count++] = (capture_frame_t){field_value, child, 0};
        } else if (field->is_comptime) {
            if (!field_value.known) {
                report_semantic_error(line, "Comptime attribute '%s' has no known value when copying an object '%s'",
                    field->key, frame->value.object ? frame->value.object->key : frame->value.object_type->key);
                continue;
            }
            const map_node_t *constant = comptime_make_constant(field_value, line);
            if (constant) {
                symbol_capture_info_t *capture = symbol_ensure_captures(frame->snapshot);
                symbol_list_append(&capture->comptime_bindings, field);
                symbol_list_append(&capture->comptime_values, (map_node_t *)constant);
            }
        }
    }
    free(stack);
    return root;
}

typedef struct {
    const map_node_t *object, *type;
    comptime_value_t *fields;
    size_t next;
    bool resumed;
} copy_frame_t;

static copy_frame_t copy_frame(const comptime_copy_t *copy, const map_node_t *object,
                               const map_node_t *type, comptime_value_t source) {
    size_t count = type->class_info->effective_attributes.count;
    copy_frame_t frame = {object, type, memory_zero(count, sizeof(*frame.fields)), 0, false};
    /* Read siblings before writing any of them, including when source aliases destination. */
    for (size_t i = 0; i < count; i++)
        if (source.object_type && i < source.object_type->class_info->effective_attributes.count)
            frame.fields[i] = comptime_object_field(source, source.object_type->class_info->effective_attributes.items[i],
                                                    copy->source);
    return frame;
}

void comptime_copy_object_fields(const comptime_copy_t *copy, const map_node_t *object,
                                 const map_node_t *type, comptime_value_t source, int line) {
    /* Source type checking has already rejected cyclic containment. */
    if (!type) return;
    size_t count = 1, capacity = 0;
    copy_frame_t *stack = grow_object_stack(NULL, &capacity, count, sizeof(*stack));
    stack[0] = copy_frame(copy, object, type, source);
    while (count) {
        copy_frame_t *frame = &stack[count - 1];
        const symbol_list_t *fields = &frame->type->class_info->effective_attributes;
        if (frame->next == fields->count) {
            free(frame->fields);
            count--;
            continue;
        }
        map_node_t *field = fields->items[frame->next];
        comptime_value_t *value = &frame->fields[frame->next];
        if (field->type_symbol) {
            const map_node_t *child = comptime_object_location(frame->object, field);
            if (!frame->resumed) {
                copy_frame_t next = copy_frame(copy, child, field->type_symbol, *value);
                frame->resumed = true;
                stack = grow_object_stack(stack, &capacity, memory_add(count, 1), sizeof(*stack));
                stack[count++] = next;
                continue;
            }
            value->object = child;
            value->object_type = field->type_symbol;
            value->object_snapshot = NULL;
        } else if (field->is_comptime) {
            if (!copy->argument && (copy->unknown_control ||
                (copy->external_unknown && frame->object->owner != copy->function)))
                report_semantic_error(line, "Cannot modify comptime '%s' under unknown control in object '%s'",
                    field->key, frame->object->key);
            else if (!value->known)
                report_semantic_error(line, copy->argument ?
                    "Comptime attribute '%s' has no known value in argument copy for object '%s'" :
                    "Value for comptime '%s' is not known at compile time in object '%s'", field->key, frame->object->key);
        }
        if (!copy->argument || field->is_comptime)
            comptime_set(copy->destination, field, frame->object, *value);
        frame->next++;
        frame->resumed = false;
    }
    free(stack);
}

void comptime_objects_init(void) {
    object_locations = NULL;
}

void comptime_objects_destroy(void) {
    while (object_locations != NULL) {
        object_location_t *next = object_locations->next;
        symbol_clear(&object_locations->symbol);
        free(object_locations);
        object_locations = next;
    }
}

const map_node_t *comptime_select_field(const map_node_t *type, comptime_value_t index) {
    if (!type || !index.known || index.floating || index.number < 0 ||
        index.number >= type->class_info->effective_attributes.count) return NULL;
    return type->class_info->effective_attributes.items[(size_t)index.number];
}
