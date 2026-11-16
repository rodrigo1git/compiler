#include "../include/specialize_internal.h"
#include "../include/comptime_objects.h"
#include "../include/classes.h"

void sp_forget_call_writes(comptime_environment_t *environment, const function_template_t *source) {
    for (size_t i = 0; i < environment->count; i++) {
        comptime_binding_t *binding = &environment->items[i];
        if (binding->symbol->is_comptime) {
            continue;
        }
        bool written =
            source->writes_attributes && (binding->object != NULL || binding->symbol->type_symbol != NULL);
        for (size_t j = 0; j < source->runtime_writes.count; j++) {
            if (source->runtime_writes.items[j] == binding->symbol ||
                source->runtime_writes.items[j] == comptime_object_root(binding->object)) {
                written = true;
            }
        }
        if (written) {
            binding->value = (comptime_value_t){0};
        }
    }
}

void sp_apply_effects(const function_template_t *source, const comptime_environment_t *after,
                      const map_node_t *from_receiver, comptime_environment_t *caller,
                      map_node_t *receiver) {
    sp_forget_call_writes(caller, source);
    for (size_t i = 0; i < after->count; i++) {
        comptime_binding_t binding = after->items[i];
        if (binding.object == from_receiver && from_receiver != NULL) {
            binding.object = receiver;
        }
        if (binding.symbol->is_comptime && source->external_writes &&
            binding.symbol->owner != source->symbol &&
            (binding.object == NULL || binding.object->owner != source->symbol)) {
            comptime_set(caller, binding.symbol, binding.object, binding.value);
        }
        if (!binding.symbol->is_comptime && binding.value.known && binding.object != NULL &&
            binding.object->owner != source->symbol) {
            comptime_set(caller, binding.symbol, binding.object, binding.value);
        }
    }
    for (size_t i = 0; i < source->runtime_writes.count; i++) {
        map_node_t *symbol = source->runtime_writes.items[i];
        if (symbol->usage != SYMBOL_ATTRIBUTE) {
            comptime_set(caller, symbol, NULL, comptime_get(after, symbol, NULL));
        }
    }
}

static bool step_has_comptime_effect(const specialize_state_t *state, const rpn_step_t *step) {
    if (step->kind == RPN_DESTINATION) {
        if (step->value.symbol->is_comptime && step->value.symbol->usage != SYMBOL_AUXILIARY) {
            return true;
        }
        if (class_has_comptime(step->value.symbol->type_symbol)) {
            return true;
        }
    }
    if (step->kind == RPN_ATTRIBUTE_DESTINATION) {
        if (step->value.symbol != NULL && step->value.symbol->is_comptime) {
            return true;
        }
        if (step->receiver != NULL && class_has_comptime(step->receiver->type_symbol)) {
            return true;
        }
    }
    if (step->kind == RPN_CALL) {
        function_template_t *source = sp_find_template(state, step->value.symbol);
        if (source != NULL && source->external_writes) {
            return true;
        }
    }
    return false;
}

bool sp_has_comptime_effects(const specialize_state_t *state, const rpn_t *input, size_t begin, size_t end) {
    for (size_t i = begin; i < end; i++) {
        if (step_has_comptime_effect(state, &input->steps[i])) {
            return true;
        }
    }
    return false;
}

static bool record_runtime_write(function_template_t *source, map_node_t *symbol) {
    if (symbol == NULL || symbol->is_comptime || symbol->owner == source->symbol) {
        return false;
    }
    for (size_t i = 0; i < source->runtime_writes.count; i++) {
        if (source->runtime_writes.items[i] == symbol) {
            return false;
        }
    }
    symbol_list_append(&source->runtime_writes, symbol);
    return true;
}

static void analyze_step_effects(function_template_t *source, const rpn_step_t *step) {
    map_node_t *symbol = step->value.symbol;
    switch (step->kind) {
        case RPN_SYMBOL:
            if (class_has_comptime(symbol->type_symbol) ||
                (symbol->is_comptime && symbol->owner != source->symbol)) {
                source->contextual = true;
            }
            break;
        case RPN_DESTINATION:
            record_runtime_write(source, symbol);
            if (symbol->usage == SYMBOL_ATTRIBUTE) {
                source->writes_attributes = true;
            }
            if (class_has_comptime(symbol->type_symbol)) {
                source->contextual = true;
                source->writes_attributes = true;
                if (symbol->owner != source->symbol) {
                    source->external_writes = true;
                }
            }
            if (symbol->is_comptime && symbol->owner != source->symbol) {
                source->contextual = true;
                source->external_writes = true;
            }
            break;
        case RPN_ATTRIBUTE:
            if (class_has_comptime(symbol != NULL ? symbol->type_symbol : NULL) ||
                class_has_comptime(step->receiver->type_symbol) ||
                (step->index != NULL && step->index->is_comptime) ||
                (symbol != NULL && symbol->is_comptime)) {
                source->contextual = true;
            }
            break;
        case RPN_ATTRIBUTE_DESTINATION:
            source->writes_attributes = true;
            if ((step->index != NULL && step->index->is_comptime) ||
                (symbol != NULL && symbol->is_comptime)) {
                source->contextual = true;
            }
            if (class_has_comptime(step->receiver->type_symbol)) {
                source->contextual = true;
                if (step->receiver->owner != source->symbol) {
                    source->external_writes = true;
                }
            }
            break;
        default:
            break;
    }
}

static void analyze_function_effects(function_template_t *source) {
    for (size_t i = 0; i < source->symbol->function_info->parameters.count; i++) {
        if (source->symbol->function_info->parameters.items[i]->is_comptime) {
            source->contextual = true;
        }
    }
    for (size_t i = 0; i < source->body->count; i++) {
        analyze_step_effects(source, &source->body->steps[i]);
    }
}

static void propagate_function_effects(specialize_state_t *state) {
    bool changed;
    do {
        changed = false;
        for (function_template_t *source = state->templates; source != NULL; source = source->next) {
            for (size_t i = 0; i < source->body->count; i++) {
                const rpn_step_t *step = &source->body->steps[i];
                if (step->kind != RPN_CALL) {
                    continue;
                }
                function_template_t *called = sp_find_template(state, step->value.symbol);
                if (called == NULL) {
                    continue;
                }
                if (called->contextual && !source->contextual) {
                    source->contextual = true;
                    changed = true;
                }
                if (called->external_writes && !source->external_writes) {
                    source->external_writes = true;
                    changed = true;
                }
                if (called->writes_attributes && !source->writes_attributes) {
                    source->writes_attributes = true;
                    changed = true;
                }
                for (size_t j = 0; j < called->runtime_writes.count; j++) {
                    if (record_runtime_write(source, called->runtime_writes.items[j])) {
                        changed = true;
                    }
                }
            }
        }
    } while (changed);
}

void sp_analyze_effects(specialize_state_t *state) {
    for (function_template_t *source = state->templates; source; source = source->next) {
        analyze_function_effects(source);
    }
    propagate_function_effects(state);
    for (function_template_t *source = state->templates; source; source = source->next) {
        source->symbol->function_info->comptime_template = source->contextual;
    }
}
