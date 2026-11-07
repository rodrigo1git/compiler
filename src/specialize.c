#include "../include/memory.h"
#include <stdlib.h>
#include "../include/specialize_internal.h"
#include "../include/comptime_objects.h"
#include "../include/diagnostics.h"

static specialize_state_t state;

function_template_t *sp_find_template(const specialize_state_t *state, const map_node_t *symbol) {
    if (symbol != NULL && symbol->source_function != NULL) symbol = symbol->source_function;
    for (function_template_t *source = state->templates; source != NULL; source = source->next)
        if (source->symbol == symbol) return source;
    return NULL;
}

void specialize_init(void) {
    state = (specialize_state_t){.evaluation_budget = SP_EVALUATION_BUDGET};
    comptime_objects_init();
    clear_semantic_origins();
    rpn_init(&state.main_body);
}

void specialize_register(map_node_t *symbol, const rpn_t *body) {
    function_template_t *source = memory_resize(NULL, sizeof(*source));
    *source = (function_template_t){.symbol = symbol, .body = body};
    if (state.last_template == NULL) state.templates = source;
    else state.last_template->next = source;
    state.last_template = source;
}

bool specialize_program(const rpn_t *program) {
    sp_analyze_effects(&state);
    comptime_environment_t environment = {0};
    sp_transform(&state, program, &state.main_body, &environment, NULL, NULL, false, NULL);
    for (function_template_t *source = state.templates; source != NULL; source = source->next) {
        if (!source->used && !source->contextual) {
            comptime_environment_t empty = {0};
            sp_instantiate(&state, source, &empty, &empty, NULL, false, source->symbol->declaration_line);
        }
    }
    for (size_t i = 0; i < environment.count; i++) {
        comptime_binding_t binding = environment.items[i];
        if (binding.symbol->is_comptime && binding.symbol->owner == NULL && binding.value.known)
            ((map_node_t *)binding.symbol)->comptime_value =
                (map_node_t *)comptime_make_constant(binding.value, binding.symbol->declaration_line);
    }
    free(environment.items);
    return global_errors == 0;
}

bool specialize_print(FILE *stream) {
    if (!rpn_is_complete(&state.main_body) || !rpn_print(stream, &state.main_body)) return false;
    for (function_instance_t *instance = state.instances; instance != NULL; instance = instance->next) {
        if (instance->active || !rpn_is_complete(&instance->body)) return false;
        if (fprintf(stream, "\n--- FUNCTION %s ---\n", instance->symbol->key) < 0 ||
            !rpn_print(stream, &instance->body))
            return false;
    }
    /* Source templates remain symbolic, even when concrete instances exist. */
    for (function_template_t *source = state.templates; source != NULL; source = source->next) {
        if (!source->contextual) continue;
        if (fprintf(stream, "\n--- TEMPLATE %s ---\n", source->symbol->key) < 0 ||
            !rpn_print(stream, source->body))
            return false;
    }
    return true;
}

comptime_value_t specialize_evaluate_call(const map_node_t *symbol, comptime_environment_t *environment,
                                          const comptime_environment_t *arguments, const map_node_t *receiver,
                                          bool unknown_entry, int line) {
    function_template_t *source = sp_find_template(&state, symbol);
    if (!source || sp_find_active_instance(&state, source)) return (comptime_value_t){0};
    /* Validation owns only abstract environments, never executable instances. */
    for (const evaluation_frame_t *frame = state.evaluating; frame; frame = frame->parent)
        if (frame->function == source->symbol) return (comptime_value_t){0};
    if (state.evaluation_depth == SP_MAX_DEPTH) {
        report_semantic_error(line, "Compile-time evaluation exceeded the recursion resource limit");
        return (comptime_value_t){0};
    }
    evaluation_frame_t frame = {source->symbol, state.evaluating};
    state.evaluating = &frame;
    state.evaluation_depth++;
    comptime_value_t value = sp_evaluate_call(&state, source, environment, arguments, receiver, unknown_entry, line);
    state.evaluation_depth--;
    state.evaluating = frame.parent;
    return value;
}

size_t specialize_source_count(void) {
    size_t count = 0;
    for (function_template_t *item = state.templates; item; item = item->next) count++;
    return count;
}

const rpn_t *specialize_source_body(size_t index, map_node_t **symbol) {
    function_template_t *item = state.templates;
    while (item && index--) item = item->next;
    if (!item) return NULL;
    *symbol = item->symbol;
    return item->body;
}

size_t specialize_instance_count(void) {
    size_t count = 0;
    for (function_instance_t *item = state.instances; item; item = item->next) count++;
    return count;
}

const rpn_t *specialize_instance_body(size_t index, map_node_t **symbol) {
    function_instance_t *item = state.instances;
    while (item && index--) item = item->next;
    if (!item) return NULL;
    *symbol = item->symbol;
    return &item->body;
}

const rpn_t *specialize_main_body(void) {
    return &state.main_body;
}

bool specialize_has_comptime_effects(const rpn_t *input, size_t begin, size_t end) {
    return sp_has_comptime_effects(&state, input, begin, end);
}

bool specialize_requires_iteration(const rpn_t *input, size_t begin, size_t end,
                                   const comptime_environment_t *environment, const map_node_t *receiver) {
    return sp_requires_iteration(&state, input, begin, end, environment, receiver);
}

void specialize_destroy(void) {
    clear_semantic_origins();
    comptime_objects_destroy();
    rpn_free(&state.main_body);
    sp_destroy_instances(&state);
    while (state.templates != NULL) {
        function_template_t *next = state.templates->next;
        free(state.templates->runtime_writes.items);
        free(state.templates);
        state.templates = next;
    }
    state = (specialize_state_t){0};
}
