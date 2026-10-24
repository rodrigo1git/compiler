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

typedef struct type_path {
    const map_node_t *type;
    const struct type_path *parent;
} type_path_t;

static object_location_t *object_locations;

static void *resize(void *pointer, size_t size) {
    void *result = realloc(pointer, size);
    if (result == NULL && size != 0) {
        fprintf(stderr, "Fatal error: Cannot allocate compile-time evaluation state\n");
        exit(1);
    }
    return result;
}

const map_node_t *comptime_object_location(const map_node_t *parent, const map_node_t *field) {
    if (parent == NULL || field == NULL) return NULL;
    for (object_location_t *item = object_locations; item != NULL; item = item->next)
        if (item->parent == parent && item->field == field) return &item->symbol;
    object_location_t *item = resize(NULL, sizeof(*item));
    *item = (object_location_t){.parent = parent, .field = field, .next = object_locations};
    size_t length = strlen(parent->key) + strlen(field->key) + 3;
    item->symbol.key = resize(NULL, length);
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

static bool type_in_path(const type_path_t *path, const map_node_t *type) {
    for (; path != NULL; path = path->parent)
        if (path->type == type) return true;
    return false;
}

comptime_value_t comptime_object_field(comptime_value_t object, const map_node_t *field,
                                       const comptime_environment_t *environment) {
    comptime_value_t result = {0};
    const map_node_t *snapshot = object.object_snapshot;
    if (snapshot != NULL) {
        for (size_t i = 0; i < snapshot->comptime_bindings.count; i++)
            if (snapshot->comptime_bindings.items[i] == field)
                return comptime_read_constant(snapshot->comptime_values.items[i]);
        for (size_t i = 0; i < snapshot->object_fields.count; i++)
            if (snapshot->object_fields.items[i] == field)
                result.object_snapshot = snapshot->object_values.items[i];
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

static map_node_t *capture_object(comptime_value_t value, const comptime_environment_t *environment,
                                  unsigned int *serial, int line, const type_path_t *path) {
    if (!class_has_comptime(value.object_type)) return NULL;
    if (type_in_path(path, value.object_type)) {
        if (value.object_snapshot != NULL) return (map_node_t *)value.object_snapshot;
        report_semantic_error(line, "Compile-time fields in recursive object type '%s' have no finite known value",
            value.object_type->key);
        return NULL;
    }
    type_path_t current = {value.object_type, path};
    char name[64];
    snprintf(name, sizeof(name), "@value%u", ++(*serial));
    map_node_t *snapshot = map_put(symbol_table, name, "ID");
    snapshot->usage = SYMBOL_OBJECT_VALUE;
    snapshot->type_symbol = (map_node_t *)value.object_type;
    snapshot->declared_type = resize(NULL, strlen(value.object_type->key) + 1);
    strcpy(snapshot->declared_type, value.object_type->key);
    snapshot->declaration_line = line;
    for (size_t i = 0; i < value.object_type->effective_attributes.count; i++) {
        map_node_t *field = value.object_type->effective_attributes.items[i];
        if (!field->is_comptime && !class_has_comptime(field->type_symbol)) continue;
        comptime_value_t field_value = comptime_object_field(value, field, environment);
        if (field->type_symbol != NULL) {
            map_node_t *child = capture_object(field_value, environment, serial, line, &current);
            if (child != NULL) {
                symbol_list_append(&snapshot->object_fields, field);
                symbol_list_append(&snapshot->object_values, child);
            }
        } else if (field->is_comptime) {
            if (!field_value.known) {
                report_semantic_error(line, "Comptime attribute '%s' has no known value when copying an object '%s'",
                    field->key, value.object != NULL ? value.object->key : value.object_type->key);
                continue;
            }
            const map_node_t *constant = comptime_make_constant(field_value, line);
            if (constant != NULL) {
                symbol_list_append(&snapshot->comptime_bindings, field);
                symbol_list_append(&snapshot->comptime_values, (map_node_t *)constant);
            }
        }
    }
    return snapshot;
}

map_node_t *comptime_capture_object(comptime_value_t value, const comptime_environment_t *environment,
                                    unsigned int *serial, int line) {
    return capture_object(value, environment, serial, line, NULL);
}

static void copy_object_fields(const comptime_copy_t *copy, const map_node_t *object,
                                const map_node_t *type, comptime_value_t source,
                                int line, const type_path_t *path) {
    if (type == NULL || type_in_path(path, type)) return;
    type_path_t current = {type, path};
    size_t count = type->effective_attributes.count;
    comptime_value_t *fields = resize(NULL, count * sizeof(*fields));
    for (size_t i = 0; i < count; i++) {
        fields[i] = (comptime_value_t){0};
        if (source.object_type != NULL && i < source.object_type->effective_attributes.count)
            fields[i] = comptime_object_field(source, source.object_type->effective_attributes.items[i],
                copy->source);
    }
    for (size_t i = 0; i < count; i++) {
        map_node_t *field = type->effective_attributes.items[i];
        if (field->type_symbol != NULL) {
            const map_node_t *child = comptime_object_location(object, field);
            copy_object_fields(copy, child, field->type_symbol, fields[i], line, &current);
            fields[i].object = child;
            fields[i].object_type = field->type_symbol;
            fields[i].object_snapshot = NULL;
        } else if (field->is_comptime) {
            if (!copy->argument && (copy->unknown_control ||
                (copy->external_unknown && object->owner != copy->function)))
                report_semantic_error(line, "Cannot modify comptime '%s' under unknown control in object '%s'",
                    field->key, object->key);
            else if (!fields[i].known)
                report_semantic_error(line, copy->argument ?
                    "Comptime attribute '%s' has no known value in argument copy for object '%s'" :
                    "Value for comptime '%s' is not known at compile time in object '%s'", field->key, object->key);
        }
        if (!copy->argument || field->is_comptime)
            comptime_set(copy->destination, field, object, fields[i]);
    }
    free(fields);
}

void comptime_copy_object_fields(const comptime_copy_t *copy, const map_node_t *object,
                                 const map_node_t *type, comptime_value_t source, int line) {
    copy_object_fields(copy, object, type, source, line, NULL);
}

void comptime_objects_init(void) {
    object_locations = NULL;
}

void comptime_objects_destroy(void) {
    while (object_locations != NULL) {
        object_location_t *next = object_locations->next;
        free(object_locations->symbol.key);
        free(object_locations);
        object_locations = next;
    }
}
