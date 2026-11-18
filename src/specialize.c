#include "../include/memory.h"
#include <stdlib.h>
#include "../include/specialize_internal.h"
#include "../include/comptime_objects.h"
#include "../include/classes.h"
#include "../include/diagnostics.h"

static specialize_state_t g_state;

function_template_t *sp_find_template(const specialize_state_t *state, const map_node_t *symbol) {
    if (symbol != NULL && symbol->function_info != NULL && symbol->function_info->source_function != NULL) {
        symbol = symbol->function_info->source_function;
    }
    for (size_t i = 0; i < state->templates.count; i++) {
        function_template_t *source = state->templates.items[i];
        if (source->symbol == symbol) {
            return source;
        }
    }
    return NULL;
}

sp_pending_call_t *sp_pending_push(sp_pending_call_t **pending, function_template_t *source,
                                   map_node_t *receiver, size_t frame_step, int errors) {
    sp_pending_call_t *call = memory_resize(NULL, sizeof(*call));
    *call = (sp_pending_call_t){
        .source = source,
        .receiver = receiver,
        .frame_step = frame_step,
        .errors = errors,
        .parent = *pending
    };
    *pending = call;
    return call;
}

void sp_pending_pop(sp_pending_call_t **pending) {
    if (*pending == NULL) {
        return;
    }
    sp_pending_call_t *parent = (*pending)->parent;
    comptime_environment_free(&(*pending)->arguments);
    free(*pending);
    *pending = parent;
}

void sp_pending_free_all(sp_pending_call_t **pending) {
    while (*pending != NULL) {
        sp_pending_pop(pending);
    }
}

void sp_record_return(sp_return_state_t *state, comptime_value_t value) {
    if (!state->has_value) {
        state->value = value;
    } else if (!comptime_values_equal(state->value, value)) {
        state->value = (comptime_value_t){0};
    }
    state->has_value = true;
}

bool sp_is_forbidden_comptime_write(const map_node_t *symbol, const map_node_t *object,
                                    const map_node_t *function, bool argument,
                                    bool unknown_control, bool external_unknown) {
    if (argument || symbol->usage == SYMBOL_AUXILIARY) {
        return false;
    }
    bool local = symbol->owner == function || (object != NULL && object->owner == function);
    return unknown_control || (external_unknown && !local);
}

bool sp_comptime_arguments_known(const function_template_t *source,
                                 const comptime_environment_t *arguments,
                                 int line, bool report) {
    bool all_known = true;
    for (size_t i = 0; i < source->symbol->function_info->parameters.count; i++) {
        const map_node_t *parameter = source->symbol->function_info->parameters.items[i];
        if (!parameter->is_comptime || comptime_get(arguments, parameter, NULL).known) {
            continue;
        }
        all_known = false;
        if (report) {
            report_semantic_error(line, "Argument for comptime parameter '%s' is not known at compile time",
                                  parameter->key);
        }
    }
    return all_known;
}

map_node_t *sp_attribute_at(map_node_t *type, comptime_value_t index, int line) {
    if (type == NULL || !index.known || index.floating) {
        return NULL;
    }
    if (index.number < 0) {
        report_semantic_error(line, "Attribute position %ld does not exist in class '%s'",
                              (long)index.number, type->key);
        return NULL;
    }
    return class_attribute_at(type, (size_t)index.number, line);
}

void specialize_init(void) {
    g_state = (specialize_state_t){0};
    comptime_objects_init();
    clear_semantic_origins();
    rpn_init(&g_state.main_body);
}

void specialize_register(map_node_t *symbol, const rpn_t *body) {
    function_template_t *source = memory_resize(NULL, sizeof(*source));
    *source = (function_template_t){.symbol = symbol, .body = body};
    if (g_state.templates.count == g_state.templates.capacity) {
        size_t capacity = g_state.templates.capacity ? memory_multiply(g_state.templates.capacity, 2) : 16;
        g_state.templates.items = memory_resize(
            g_state.templates.items, memory_multiply(capacity, sizeof(*g_state.templates.items)));
        g_state.templates.capacity = capacity;
    }
    g_state.templates.items[g_state.templates.count++] = source;
}

bool specialize_program(const rpn_t *program) {
    sp_analyze_effects(&g_state);
    comptime_environment_t environment = {0};
    sp_transform_request_t main_request = {
        .state = &g_state,
        .input = program,
        .output = &g_state.main_body,
        .environment = &environment
    };
    sp_transform(&main_request);
    for (size_t i = 0; i < g_state.templates.count; i++) {
        function_template_t *source = g_state.templates.items[i];
        if (!source->used && !source->contextual) {
            comptime_environment_t empty = {0};
            sp_instantiate(&g_state, source, &empty, &empty, NULL, false, source->symbol->declaration_line);
        }
    }
    for (size_t i = 0; i < environment.count; i++) {
        comptime_binding_t binding = environment.items[i];
        if (binding.symbol->is_comptime && binding.symbol->owner == NULL && binding.value.known) {
            binding.symbol->comptime_value =
                comptime_make_constant(binding.value, binding.symbol->declaration_line);
        }
    }
    comptime_environment_free(&environment);
    return global_errors == 0;
}

bool specialize_print(FILE *stream) {
    if (!rpn_is_complete(&g_state.main_body) || !rpn_print(stream, &g_state.main_body)) {
        return false;
    }
    for (size_t i = 0; i < g_state.instances.count; i++) {
        function_instance_t *instance = g_state.instances.items[i];
        if (instance->source->active_instance == instance || !rpn_is_complete(&instance->body)) {
            return false;
        }
        if (fprintf(stream, "\n--- FUNCTION %s ---\n", instance->symbol->key) < 0 ||
            !rpn_print(stream, &instance->body)) {
            return false;
        }
    }
    for (size_t i = 0; i < g_state.templates.count; i++) {
        function_template_t *source = g_state.templates.items[i];
        if (!source->contextual) {
            continue;
        }
        if (fprintf(stream, "\n--- TEMPLATE %s ---\n", source->symbol->key) < 0 ||
            !rpn_print(stream, source->body)) {
            return false;
        }
    }
    return true;
}

size_t specialize_source_count(void) {
    return g_state.templates.count;
}

const rpn_t *specialize_source_body(size_t index, map_node_t **symbol) {
    if (index >= g_state.templates.count) {
        return NULL;
    }
    function_template_t *item = g_state.templates.items[index];
    *symbol = item->symbol;
    return item->body;
}

size_t specialize_instance_count(void) {
    return g_state.instances.count;
}

const rpn_t *specialize_instance_body(size_t index, map_node_t **symbol) {
    if (index >= g_state.instances.count) {
        return NULL;
    }
    function_instance_t *item = g_state.instances.items[index];
    *symbol = item->symbol;
    return &item->body;
}

const rpn_t *specialize_main_body(void) {
    return &g_state.main_body;
}

void specialize_destroy(void) {
    clear_semantic_origins();
    comptime_objects_destroy();
    rpn_free(&g_state.main_body);
    sp_destroy_instances(&g_state);
    for (size_t i = 0; i < g_state.templates.count; i++) {
        function_template_t *source = g_state.templates.items[i];
        free(source->runtime_writes.items);
        free(source);
    }
    free(g_state.templates.items);
    g_state = (specialize_state_t){0};
}
