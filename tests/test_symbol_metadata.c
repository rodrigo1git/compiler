#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"

/* Metadata owns containers, while graph edges borrow the table's symbols.
   Run under ASan as well to check arbitrary hash-order destruction and symbols
   embedded in stack/heap objects rather than allocated as standalone nodes. */
int main(void) {
    hash_map_t *table = map_create(8);
    map_node_t *parameter = map_put(table, "p:main:f", "ID");
    symbol_set_usage(parameter, SYMBOL_PARAMETER);
    parameter->declared_type = strdup("INTEGER");
    assert(parameter->class_info == NULL && parameter->function_info == NULL &&
           parameter->capture_info == NULL);

    map_node_t *kind = map_put(table, "box:main", "ID");
    symbol_set_usage(kind, SYMBOL_CLASS);
    kind->class_info->class_code = strdup("boxcode");
    symbol_list_append(&kind->class_info->attributes, parameter);
    symbol_list_append(&kind->class_info->effective_attributes, parameter);
    symbol_class_info_t *class_info = kind->class_info;
    symbol_set_usage(kind, SYMBOL_CLASS);
    assert(kind->class_info == class_info && class_info->attributes.count == 1);
    assert(map_get(table, "box:main", "ID") == kind);

    map_node_t *object = map_put(table, "object:main", "ID");
    symbol_set_usage(object, SYMBOL_VARIABLE);
    object->type_symbol = kind;
    assert(object->class_info == NULL && object->function_info == NULL &&
           object->capture_info == NULL);

    map_node_t *source = map_put(table, "f:main", "ID");
    map_node_t *instance = map_put(table, "f:main$ct1", "ID");
    symbol_set_usage(source, SYMBOL_FUNCTION);
    symbol_set_usage(instance, SYMBOL_FUNCTION);
    instance->function_info->source_function = source;
    symbol_list_append(&source->function_info->parameters, parameter);
    symbol_list_append(&instance->function_info->parameters, parameter);
    assert(source->function_info != instance->function_info);
    assert(source->function_info->parameters.items != instance->function_info->parameters.items);
    symbol_list_append(&instance->function_info->parameters, object);
    assert(source->function_info->parameters.count == 1);
    assert(source->capture_info == NULL && instance->capture_info == NULL);

    map_node_t *constant = map_put(table, "3$i", "INTEGER");
    map_node_t *snapshot = map_put(table, "@value1", "ID");
    symbol_set_usage(snapshot, SYMBOL_OBJECT_VALUE);
    snapshot->type_symbol = kind;
    symbol_capture_info_t *captured = symbol_ensure_captures(snapshot);
    symbol_list_append(&captured->comptime_bindings, parameter);
    symbol_list_append(&captured->comptime_values, constant);
    symbol_capture_info_t *bindings = symbol_ensure_captures(instance);
    symbol_list_append(&bindings->comptime_bindings, parameter);
    symbol_list_append(&bindings->comptime_values, constant);
    symbol_list_append(&bindings->object_fields, parameter);
    symbol_list_append(&bindings->object_values, snapshot);
    assert(bindings != captured && instance->function_info != NULL);
    assert(symbol_ensure_captures(instance) == bindings);
    assert(source->capture_info == NULL);

    map_node_t embedded = {.key = strdup("embedded")};
    symbol_set_usage(&embedded, SYMBOL_METHOD);
    symbol_list_append(&embedded.function_info->parameters, parameter);
    symbol_list_append(&symbol_ensure_captures(&embedded)->object_fields, object);
    symbol_list_append(&embedded.capture_info->object_values, snapshot);
    symbol_clear(&embedded);
    symbol_clear(&embedded);
    symbol_clear(NULL);
    assert(embedded.key == NULL && embedded.function_info == NULL && embedded.capture_info == NULL);
    assert(strcmp(parameter->declared_type, "INTEGER") == 0);
    assert(map_get(table, "@value1", "ID") == snapshot);
    assert(source->function_info->parameters.items[0] == parameter);

    printf("Symbol metadata ownership passed; sizes: symbol=%zu, class=%zu, function=%zu, captures=%zu.\n",
        sizeof(map_node_t), sizeof(symbol_class_info_t), sizeof(symbol_function_info_t),
        sizeof(symbol_capture_info_t));
    map_free(table);
    return 0;
}
