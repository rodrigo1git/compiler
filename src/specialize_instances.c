#include "../include/memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/specialize_internal.h"
#include "../include/diagnostics.h"

static bool matches_environment(const function_instance_t *instance,
                                const comptime_environment_t *environment, const map_node_t *receiver,
                                bool unknown_entry) {
    if (instance->receiver != receiver) {
        return false;
    }
    if (instance->unknown_entry != unknown_entry && instance->source->external_writes) {
        return false;
    }
    for (size_t i = 0; i < instance->before.count; i++) {
        comptime_binding_t binding = instance->before.items[i];
        if (binding.symbol->is_comptime &&
            !comptime_values_equal(binding.value, comptime_get(environment, binding.symbol, binding.object))) {
            return false;
        }
    }
    for (size_t i = 0; i < environment->count; i++) {
        comptime_binding_t binding = environment->items[i];
        if (binding.symbol->is_comptime &&
            !comptime_values_equal(binding.value,
                                   comptime_get(&instance->before, binding.symbol, binding.object))) {
            return false;
        }
    }
    return true;
}

static comptime_environment_t prepare_call_environment(const function_template_t *source,
                                                       const comptime_environment_t *caller,
                                                       const comptime_environment_t *arguments, int line) {
    comptime_environment_t environment = comptime_call_environment(caller, arguments, source->symbol);
    sp_comptime_arguments_known(source, arguments, line, true);
    return environment;
}

static function_instance_t *find_instance(specialize_state_t *state, function_template_t *source,
                                          const comptime_environment_t *environment,
                                          const map_node_t *receiver, bool unknown_entry) {
    for (size_t i = 0; i < state->instances.count; i++) {
        function_instance_t *instance = state->instances.items[i];
        if (instance->source == source &&
            (!source->contextual || matches_environment(instance, environment, receiver, unknown_entry))) {
            return instance;
        }
    }
    return NULL;
}

static function_instance_t *create_instance(specialize_state_t *state, function_template_t *source,
                                            const comptime_environment_t *environment,
                                            map_node_t *receiver, bool unknown_entry) {
    function_instance_t *instance = memory_resize(NULL, sizeof(*instance));
    *instance = (function_instance_t){
        .source = source,
        .symbol = source->symbol,
        .before = comptime_copy_environment(environment),
        .unknown_entry = unknown_entry,
        .receiver = receiver
    };
    rpn_init(&instance->body);
    if (source->contextual) {
        unsigned int serial = ++state->serial;
        int needed = snprintf(NULL, 0, "%s$ct%u", source->symbol->key, serial);
        size_t length = memory_add((size_t)(needed < 0 ? 0 : needed), 1);
        char *name = memory_resize(NULL, length);
        snprintf(name, length, "%s$ct%u", source->symbol->key, serial);
        instance->symbol = map_put(symbol_table, name, "ID");
        free(name);
        symbol_set_usage(instance->symbol, source->symbol->usage);
        instance->symbol->function_info->source_function = source->symbol;
        instance->symbol->declared_type = memory_strdup(source->symbol->declared_type);
        instance->symbol->declaration_line = source->symbol->declaration_line;
        for (size_t i = 0; i < source->symbol->function_info->parameters.count; i++) {
            symbol_list_append(&instance->symbol->function_info->parameters,
                               source->symbol->function_info->parameters.items[i]);
        }
    }
    if (state->instances.count == state->instances.capacity) {
        size_t capacity = state->instances.capacity ? memory_multiply(state->instances.capacity, 2) : 16;
        state->instances.items = memory_resize(
            state->instances.items, memory_multiply(capacity, sizeof(*state->instances.items)));
        state->instances.capacity = capacity;
    }
    state->instances.items[state->instances.count++] = instance;
    return instance;
}

static void record_instance_bindings(function_instance_t *instance) {
    function_template_t *source = instance->source;
    const comptime_environment_t *environment = &instance->after;
    for (size_t i = 0; i < environment->count; i++) {
        comptime_binding_t binding = environment->items[i];
        if (!binding.symbol->is_comptime || !binding.value.known || binding.symbol->owner != source->symbol) {
            continue;
        }
        map_node_t *constant = comptime_make_constant(binding.value, binding.symbol->declaration_line);
        if (constant == NULL) {
            continue;
        }
        symbol_capture_info_t *capture = symbol_ensure_captures(instance->symbol);
        symbol_list_append(&capture->comptime_bindings, binding.symbol);
        symbol_list_append(&capture->comptime_values, constant);
        if (!source->contextual) {
            binding.symbol->comptime_value = constant;
        }
    }
}

function_instance_t *sp_instantiate(specialize_state_t *state, function_template_t *source,
                                    comptime_environment_t *caller, const comptime_environment_t *arguments,
                                    map_node_t *receiver, bool unknown_entry, int line) {
    comptime_environment_t environment = prepare_call_environment(source, caller, arguments, line);
    source->used = true;
    function_instance_t *instance = find_instance(state, source, &environment, receiver, unknown_entry);
    if (instance != NULL) {
        comptime_environment_free(&environment);
        return instance;
    }
    assert(source->active_instance == NULL);
    instance = create_instance(state, source, &environment, receiver, unknown_entry);
    source->active_instance = instance;
    sp_transform_request_t request = {
        .state = state,
        .input = source->body,
        .output = &instance->body,
        .environment = &environment,
        .function = source->symbol,
        .receiver = receiver,
        .external_unknown = unknown_entry,
        .returned = &instance->returned
    };
    sp_transform(&request);
    source->active_instance = NULL;
    instance->after = environment;
    record_instance_bindings(instance);
    return instance;
}

void sp_destroy_instances(specialize_state_t *state) {
    for (size_t i = 0; i < state->instances.count; i++) {
        function_instance_t *instance = state->instances.items[i];
        rpn_free(&instance->body);
        comptime_environment_free(&instance->before);
        comptime_environment_free(&instance->after);
        free(instance);
    }
    free(state->instances.items);
    state->instances = (sp_instance_list_t){0};
}
