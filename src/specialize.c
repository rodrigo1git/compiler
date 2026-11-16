#include "../include/memory.h"
#include <stdlib.h>
#include "../include/specialize_internal.h"
#include "../include/comptime_objects.h"
#include "../include/diagnostics.h"

static specialize_state_t g_state;

function_template_t *sp_find_template(const specialize_state_t *state, const map_node_t *symbol) {
    if (symbol != NULL && symbol->function_info != NULL && symbol->function_info->source_function != NULL) {
        symbol = symbol->function_info->source_function;
    }
    for (function_template_t *source = state->templates; source != NULL; source = source->next) {
        if (source->symbol == symbol) {
            return source;
        }
    }
    return NULL;
}

void sp_pending_free_all(sp_pending_call_t **pending) {
    while (*pending != NULL) {
        sp_pending_call_t *parent = (*pending)->parent;
        comptime_environment_free(&(*pending)->arguments);
        free(*pending);
        *pending = parent;
    }
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

void specialize_init(void) {
    g_state = (specialize_state_t){0};
    comptime_objects_init();
    clear_semantic_origins();
    rpn_init(&g_state.main_body);
}

void specialize_register(map_node_t *symbol, const rpn_t *body) {
    function_template_t *source = memory_resize(NULL, sizeof(*source));
    *source = (function_template_t){.symbol = symbol, .body = body};
    if (g_state.last_template == NULL) {
        g_state.templates = source;
    } else {
        g_state.last_template->next = source;
    }
    g_state.last_template = source;
    g_state.template_count++;
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
    for (function_template_t *source = g_state.templates; source != NULL; source = source->next) {
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
    for (function_instance_t *instance = g_state.instances; instance != NULL; instance = instance->next) {
        if (instance->active || !rpn_is_complete(&instance->body)) {
            return false;
        }
        if (fprintf(stream, "\n--- FUNCTION %s ---\n", instance->symbol->key) < 0 ||
            !rpn_print(stream, &instance->body)) {
            return false;
        }
    }
    for (function_template_t *source = g_state.templates; source != NULL; source = source->next) {
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
    return g_state.template_count;
}

const rpn_t *specialize_source_body(size_t index, map_node_t **symbol) {
    function_template_t *item = g_state.templates;
    while (item != NULL && index--) {
        item = item->next;
    }
    if (item == NULL) {
        return NULL;
    }
    *symbol = item->symbol;
    return item->body;
}

size_t specialize_instance_count(void) {
    return g_state.instance_count;
}

const rpn_t *specialize_instance_body(size_t index, map_node_t **symbol) {
    function_instance_t *item = g_state.instances;
    while (item != NULL && index--) {
        item = item->next;
    }
    if (item == NULL) {
        return NULL;
    }
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
    while (g_state.templates != NULL) {
        function_template_t *next = g_state.templates->next;
        free(g_state.templates->runtime_writes.items);
        free(g_state.templates);
        g_state.templates = next;
    }
    g_state = (specialize_state_t){0};
}
