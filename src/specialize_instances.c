#include "../include/memory.h"
#include <stdlib.h>
#include <string.h>
#include "../include/specialize_internal.h"
#include "../include/diagnostics.h"

function_instance_t *sp_find_active_instance(const specialize_state_t *state,
                                             const function_template_t *source) {
    for (function_instance_t *instance = state->instances; instance; instance = instance->next)
        if (instance->active && instance->source == source) return instance;
    return NULL;
}

static bool matches_environment(const function_instance_t *instance,
                                const comptime_environment_t *environment, const map_node_t *receiver,
                                bool unknown_entry) {
    if (instance->receiver != receiver) return false;
    if (instance->unknown_entry != unknown_entry && instance->source->external_writes) return false;
    for (size_t i = 0; i < instance->before.count; i++) {
        comptime_binding_t binding = instance->before.items[i];
        if (binding.symbol->is_comptime &&
            !comptime_values_equal(binding.value, comptime_get(environment, binding.symbol, binding.object)))
            return false;
    }
    for (size_t i = 0; i < environment->count; i++) {
        comptime_binding_t binding = environment->items[i];
        if (binding.symbol->is_comptime &&
            !comptime_values_equal(binding.value,
                                   comptime_get(&instance->before, binding.symbol, binding.object)))
            return false;
    }
    return true;
}

static comptime_environment_t prepare_call_environment(const function_template_t *source,
                                                       const comptime_environment_t *caller,
                                                       const comptime_environment_t *arguments, int line) {
    comptime_environment_t environment = comptime_call_environment(caller, arguments, source->symbol);
    for (size_t i = 0; i < source->symbol->function_info->parameters.count; i++) {
        map_node_t *parameter = source->symbol->function_info->parameters.items[i];
        if (!parameter->is_comptime) continue;
        comptime_value_t argument = comptime_get(arguments, parameter, NULL);
        if (!argument.known)
            report_semantic_error(line, "Argument for comptime parameter '%s' is not known at compile time",
                                  parameter->key);
        comptime_set(&environment, parameter, NULL, argument);
    }
    return environment;
}

static function_instance_t *find_instance(specialize_state_t *state, function_template_t *source,
                                          const comptime_environment_t *environment,
                                          const map_node_t *receiver, bool unknown_entry) {
    for (function_instance_t *instance = state->instances; instance != NULL; instance = instance->next)
        if (instance->source == source &&
            (!source->contextual || matches_environment(instance, environment, receiver, unknown_entry)))
            return instance;
    return NULL;
}

static function_instance_t *create_instance(specialize_state_t *state, function_template_t *source,
                                            const comptime_environment_t *environment,
                                            const map_node_t *receiver, bool unknown_entry) {
    function_instance_t *instance = memory_resize(NULL, sizeof(*instance));
    *instance = (function_instance_t){.source = source,
                                      .symbol = source->symbol,
                                      .before = comptime_copy_environment(environment),
                                      .active = true,
                                      .unknown_entry = unknown_entry,
                                      .receiver = receiver};
    rpn_init(&instance->body);
    if (source->contextual) {
        size_t length = strlen(source->symbol->key) + 32;
        char *name = memory_resize(NULL, length);
        snprintf(name, length, "%s$ct%u", source->symbol->key, ++state->serial);
        instance->symbol = map_put(symbol_table, name, "ID");
        free(name);
        symbol_set_usage(instance->symbol, source->symbol->usage);
        instance->symbol->function_info->source_function = source->symbol;
        instance->symbol->declared_type = strdup(source->symbol->declared_type);
        if (instance->symbol->declared_type == NULL) {
            fprintf(stderr, "Fatal error: Cannot allocate specialization type\n");
            exit(1);
        }
        instance->symbol->declaration_line = source->symbol->declaration_line;
        for (size_t i = 0; i < source->symbol->function_info->parameters.count; i++)
            symbol_list_append(&instance->symbol->function_info->parameters, source->symbol->function_info->parameters.items[i]);
    }
    if (state->last_instance == NULL) state->instances = instance;
    else state->last_instance->next = instance;
    state->last_instance = instance;
    return instance;
}

static void record_instance_bindings(function_instance_t *instance) {
    function_template_t *source = instance->source;
    const comptime_environment_t *environment = &instance->after;
    for (size_t i = 0; i < environment->count; i++) {
        comptime_binding_t binding = environment->items[i];
        if (!binding.symbol->is_comptime || !binding.value.known || binding.symbol->owner != source->symbol)
            continue;
        const map_node_t *constant = comptime_make_constant(binding.value, binding.symbol->declaration_line);
        if (constant == NULL) continue;
        symbol_capture_info_t *capture = symbol_ensure_captures(instance->symbol);
        symbol_list_append(&capture->comptime_bindings, (map_node_t *)binding.symbol);
        symbol_list_append(&capture->comptime_values, (map_node_t *)constant);
        if (!source->contextual) ((map_node_t *)binding.symbol)->comptime_value = (map_node_t *)constant;
    }
}

function_instance_t *sp_instantiate(specialize_state_t *state, function_template_t *source,
                                    comptime_environment_t *caller, const comptime_environment_t *arguments,
                                    const map_node_t *receiver, bool unknown_entry, int line) {
    comptime_environment_t environment = prepare_call_environment(source, caller, arguments, line);
    source->used = true;
    function_instance_t *instance = find_instance(state, source, &environment, receiver, unknown_entry);
    if (instance != NULL) {
        free(environment.items);
        return instance;
    }
    instance = create_instance(state, source, &environment, receiver, unknown_entry);
    sp_transform(state, source->body, &instance->body, &environment, source->symbol, receiver, unknown_entry,
                 &instance->returned);
    instance->active = false;
    instance->after = environment;
    record_instance_bindings(instance);
    return instance;
}

void sp_destroy_instances(specialize_state_t *state) {
    while (state->instances != NULL) {
        function_instance_t *instance = state->instances;
        state->instances = instance->next;
        rpn_free(&instance->body);
        free(instance->before.items);
        free(instance->after.items);
        free(instance);
    }
    state->last_instance = NULL;
}
