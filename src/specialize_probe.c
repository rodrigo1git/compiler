#include "../include/diagnostics.h"
#include "../include/types.h"
#include "../include/classes.h"
#include "../include/memory.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../include/specialize_internal.h"
#include "../include/comptime_objects.h"

typedef struct {
    comptime_value_t value;
    sp_destination_t destination;
    comptime_environment_t fields;
} probe_operand_t;

typedef struct {
    bool needs_iteration;
    bool validating;
    bool auditing_types;
    bool unknown_control;
    bool external_unknown;
    bool stopped;
} probe_flags_t;

struct probe {
    specialize_state_t *state;
    const rpn_t *input;
    rpn_t resolved;
    size_t *controls;
    comptime_environment_t environment;
    map_node_t *receiver;
    probe_operand_t *stack;
    size_t count;
    sp_pending_call_t *pending;
    probe_flags_t flags;
    sp_return_state_t returns;
    map_node_t *function;
    int line;
};

static void probe_operand_free(probe_operand_t *operand) {
    comptime_environment_free(&operand->fields);
}

static void probe_validate_object(probe_t *probe, comptime_value_t value) {
    if (!class_has_comptime(value.object_type)) {
        return;
    }
    comptime_copy_t copy = {
        .source = &probe->environment,
        .function = probe->function,
        .argument = true,
        .capture = true,
        .silent = !probe->flags.validating
    };
    if (!comptime_copy_object_fields(&copy, value.object, value.object_type, value, probe->line)) {
        probe->flags.needs_iteration = true;
    }
}

static void probe_push(probe_t *probe, probe_operand_t value) {
    if (probe->flags.validating) {
        probe_validate_object(probe, value.value);
    }
    probe->stack = memory_resize(probe->stack, (probe->count + 1) * sizeof(*probe->stack));
    if (value.value.object_type != NULL) {
        value.fields = comptime_copy_environment(&probe->environment);
    }
    probe->stack[probe->count++] = value;
}

static probe_operand_t probe_pop(probe_t *probe) {
    if (probe->count != 0) {
        return probe->stack[--probe->count];
    }
    return (probe_operand_t){0};
}

static void probe_destroy(probe_t *probe) {
    for (size_t i = 0; i < probe->count; i++) {
        probe_operand_free(&probe->stack[i]);
    }
    sp_pending_free_all(&probe->pending);
    free(probe->resolved.steps);
    free(probe->controls);
    free(probe->stack);
    comptime_environment_free(&probe->environment);
}

static comptime_value_t probe_invoke(probe_t *probe, sp_pending_call_t *call) {
    function_template_t *source = call->source;
    if (source == NULL) {
        return (comptime_value_t){0};
    }
    if (probe->flags.auditing_types && call->errors != global_errors) {
        return (comptime_value_t){0};
    }
    if (probe->flags.validating || probe->flags.auditing_types) {
        return sp_evaluate_call(probe->state, source, &probe->environment, &call->arguments,
                                call->receiver,
                                probe->flags.unknown_control || probe->flags.external_unknown,
                                probe->line);
    }
    if (!sp_comptime_arguments_known(source, &call->arguments, probe->line, false)) {
        probe->flags.needs_iteration = true;
    }
    if (source->exploring || probe->flags.needs_iteration) {
        sp_forget_call_writes(&probe->environment, source);
        return (comptime_value_t){0};
    }
    source->exploring = true;
    probe_t callee = {
        .state = probe->state,
        .input = source->body,
        .receiver = call->receiver,
        .environment = comptime_call_environment(&probe->environment, &call->arguments, source->symbol)
    };
    sp_probe_range(&callee, 0, source->body->count, true);
    probe->flags.needs_iteration |= callee.flags.needs_iteration;
    sp_apply_effects(source, &callee.environment, call->receiver, &probe->environment, call->receiver);
    comptime_value_t returned = callee.returns.value;
    probe_destroy(&callee);
    source->exploring = false;
    return returned;
}

static void probe_assign(probe_t *probe, sp_destination_t destination, const probe_operand_t *assigned) {
    bool argument = destination.kind == RPN_ARGUMENT_DESTINATION && probe->pending != NULL;
    comptime_environment_t *environment = argument ? &probe->pending->arguments : &probe->environment;
    if (destination.symbol != NULL) {
        if (probe->flags.validating && destination.symbol->is_comptime) {
            bool forbidden = sp_is_forbidden_comptime_write(
                destination.symbol, destination.object, probe->function, argument,
                probe->flags.unknown_control, probe->flags.external_unknown);
            comptime_validate_assignment(destination.symbol, assigned->value, forbidden, probe->line);
        }
        if (destination.symbol->type_symbol != NULL) {
            map_node_t *object = destination.object != NULL
                ? comptime_object_location(destination.object, destination.symbol)
                : destination.symbol;
            comptime_copy_t copy = {
                .source = &assigned->fields,
                .destination = environment,
                .function = probe->function,
                .argument = argument,
                .capture = !probe->flags.auditing_types,
                .silent = !probe->flags.validating && !probe->flags.auditing_types,
                .unknown_control = probe->flags.unknown_control,
                .external_unknown = probe->flags.external_unknown
            };
            bool all_known = comptime_copy_object_fields(
                &copy, object, destination.symbol->type_symbol, assigned->value, probe->line);
            if (!all_known && !probe->flags.auditing_types) {
                probe->flags.needs_iteration = true;
            }
        }
        comptime_set(environment, destination.symbol, destination.object, assigned->value);
    } else if (!argument && !probe->flags.auditing_types) {
        comptime_invalidate_object(environment, destination.object);
    }
}

static void probe_operator(probe_t *probe, rpn_operator_t op) {
    probe_operand_t operand = {0};
    probe_operand_t right = probe_pop(probe);
    if (op <= RPN_NOT_EQUAL) {
        probe_operand_t left = probe_pop(probe);
        if (op == RPN_ASSIGN) {
            probe_assign(probe, right.destination, &left);
        } else {
            operand.value = comptime_binary(left.value, right.value, op);
            probe_push(probe, operand);
        }
        probe_operand_free(&left);
    } else if (op == RPN_TO_INTEGER) {
        operand.value = comptime_to_integer(right.value);
        probe_push(probe, operand);
    } else if (op == RPN_DUPLICATE) {
        operand.value = right.value;
        probe_push(probe, operand);
        probe_push(probe, operand);
    } else if (op == RPN_RETURN && !probe->flags.auditing_types) {
        sp_record_return(&probe->returns, right.value);
        probe->flags.stopped = true;
    }
    probe_operand_free(&right);
}

static void probe_audit_attribute_index(rpn_step_t *step, map_node_t *type, comptime_value_t index) {
    if (step->index->is_comptime) {
        if (index.known) {
            step->index = comptime_make_constant(index, step->source_line);
        } else {
            report_semantic_error(step->source_line,
                "Cannot determine comptime index '%s' when checking discarded source",
                step->index->key);
        }
    }
    if (step->index != NULL && step->index->usage == SYMBOL_LEXEME) {
        sp_attribute_at(type, index, step->source_line);
    }
}

static void probe_attribute(probe_t *probe, rpn_step_t *step) {
    probe_operand_t operand = {0};
    map_node_t *object = comptime_resolve_object(step->receiver, probe->receiver);
    map_node_t *type = object != NULL ? object->type_symbol : step->receiver->type_symbol;
    comptime_value_t index = comptime_read_symbol(&probe->environment, step->index, probe->receiver);
    if (probe->flags.auditing_types) {
        probe_audit_attribute_index(step, type, index);
    } else if (step->index->is_comptime && !index.known) {
        probe->flags.needs_iteration = true;
    }
    map_node_t *field = comptime_select_field(type, index);
    if (probe->flags.validating && step->kind == RPN_ATTRIBUTE &&
        !step->index->is_comptime && step->index->usage != SYMBOL_LEXEME) {
        probe_validate_object(probe, (comptime_value_t){
            .object = object,
            .object_type = type
        });
    }
    if (step->kind == RPN_ATTRIBUTE_DESTINATION) {
        operand.destination = (sp_destination_t){
            .symbol = field,
            .object = object,
            .kind = step->kind
        };
    } else if (field != NULL) {
        operand.value = comptime_object_field(
            (comptime_value_t){.object = object, .object_type = type},
            field, &probe->environment);
    }
    if (probe->flags.validating && step->kind == RPN_ATTRIBUTE && field != NULL &&
        field->is_comptime && !operand.value.known) {
        report_semantic_error(step->source_line, "Comptime attribute '%s' has no known value",
                              field->key);
    }
    probe_push(probe, operand);
}

static void probe_step(probe_t *probe, size_t pc) {
    rpn_step_t local_step = probe->input->steps[pc];
    rpn_step_t *step = &local_step;
    if (probe->flags.auditing_types) {
        probe->resolved.steps[pc] = local_step;
        step = &probe->resolved.steps[pc];
    }
    probe_operand_t operand = {0};
    probe->line = step->source_line;
    switch (step->kind) {
        case RPN_SYMBOL:
            operand.value = comptime_read_symbol(&probe->environment, step->value.symbol, probe->receiver);
            if (probe->flags.validating && step->value.symbol->is_comptime && !operand.value.known) {
                report_semantic_error(step->source_line, "Comptime '%s' has no known value",
                                      step->value.symbol->key);
            }
            operand.value.object_type = step->value.symbol->type_symbol;
            if (operand.value.object_type != NULL) {
                operand.value.object = comptime_resolve_object(step->value.symbol, probe->receiver);
            }
            probe_push(probe, operand);
            break;
        case RPN_DESTINATION:
        case RPN_ARGUMENT_DESTINATION:
            operand.destination = (sp_destination_t){
                .symbol = step->value.symbol,
                .kind = step->kind,
                .object = step->value.symbol->usage == SYMBOL_ATTRIBUTE ? probe->receiver : NULL
            };
            probe_push(probe, operand);
            break;
        case RPN_ATTRIBUTE:
        case RPN_ATTRIBUTE_DESTINATION:
            probe_attribute(probe, step);
            break;
        case RPN_CALL_FRAME:
            sp_pending_push(&probe->pending, sp_find_template(probe->state, step->value.symbol),
                            comptime_resolve_object(step->receiver, probe->receiver), 0, global_errors);
            break;
        case RPN_CALL: {
            if (probe->pending == NULL) {
                break;
            }
            operand.value = probe_invoke(probe, probe->pending);
            sp_pending_pop(&probe->pending);
            probe_push(probe, operand);
            break;
        }
        case RPN_OPERATOR:
            probe_operator(probe, step->value.operator);
            break;
        case RPN_INDEX_CHECK:
        case RPN_TARGET:
        case RPN_OBJECT_VALUE:
            break;
    }
}

static comptime_value_t probe_condition(probe_t *probe, rpn_control_t control) {
    sp_probe_range(probe, control.start, control.branch, false);
    probe_operand_t condition = probe_pop(probe);
    comptime_value_t value = condition.value;
    probe_operand_free(&condition);
    return value;
}

static void probe_audit_loop(probe_t *probe, rpn_control_t control) {
    int errors = global_errors;
    for (;;) {
        comptime_environment_t before = comptime_copy_environment(&probe->environment);
        sp_probe_range(probe, control.branch + 2, control.body_end, true);
        if (errors == global_errors) {
            types_check_slice(&probe->resolved, probe->function, probe->input,
                              control.branch + 2, control.body_end);
        }
        if (errors != global_errors) {
            comptime_environment_free(&before);
            return;
        }
        comptime_value_t condition = probe_condition(probe, control);
        if (errors == global_errors) {
            types_check_slice(&probe->resolved, probe->function, probe->input,
                              control.start, control.branch + 2);
        }
        bool stable = comptime_environments_equal(&before, &probe->environment);
        if (!condition.known) {
            comptime_environment_assign_owned(&probe->environment,
                comptime_join_environments(&before, &probe->environment));
        }
        comptime_environment_free(&before);
        if (errors != global_errors || !condition.known || condition.number == 0 || stable) {
            return;
        }
    }
}

static void probe_audit_control(probe_t *probe, rpn_control_t control, comptime_value_t condition) {
    if (control.loop && condition.known && condition.number != 0 &&
        (sp_has_comptime_effects(probe->state, probe->input, control.branch + 2, control.body_end) ||
         sp_requires_iteration(probe->state, probe->input, control.branch + 2, control.body_end,
                               &probe->environment, probe->receiver))) {
        if (types_check_slice(&probe->resolved, probe->function, probe->input,
                              control.start, control.branch + 2)) {
            probe_audit_loop(probe, control);
        }
        return;
    }
    comptime_environment_t original = comptime_copy_environment(&probe->environment);
    sp_probe_range(probe, control.branch + 2, control.body_end, true);
    comptime_environment_t true_environment = probe->environment;
    probe->environment = comptime_copy_environment(&original);
    if (control.has_else) {
        sp_probe_range(probe, control.false_start, control.end, true);
    }
    comptime_environment_t result;
    if (condition.known && (!control.loop || condition.number == 0)) {
        result = comptime_copy_environment(
            condition.number != 0 ? &true_environment : &probe->environment);
    } else {
        result = comptime_join_environments(&true_environment, &probe->environment);
    }
    comptime_environment_free(&original);
    comptime_environment_free(&true_environment);
    comptime_environment_assign_owned(&probe->environment, result);
}

static void probe_loop(probe_t *probe, rpn_control_t control, comptime_value_t condition) {
    if (condition.known && condition.number == 0) {
        return;
    }
    probe_t attempt = {
        .state = probe->state,
        .input = probe->input,
        .receiver = probe->receiver,
        .environment = comptime_copy_environment(&probe->environment)
    };
    comptime_forget_runtime(&attempt.environment);
    sp_probe_range(&attempt, control.branch + 2, control.body_end, true);
    bool needs_iteration = attempt.flags.needs_iteration;
    bool expand = needs_iteration ||
                  sp_has_comptime_effects(probe->state, probe->input, control.branch + 2, control.body_end);
    probe_destroy(&attempt);
    if (!expand || !condition.known) {
        if (probe->flags.validating) {
            bool previous = probe->flags.unknown_control;
            probe->flags.unknown_control = true;
            comptime_forget_runtime(&probe->environment);
            sp_probe_range(probe, control.branch + 2, control.body_end, true);
            probe->flags.unknown_control = previous;
            probe->flags.stopped = false;
        }
        probe->flags.needs_iteration |= needs_iteration;
        comptime_forget_runtime(&probe->environment);
        return;
    }
    while (condition.known && condition.number != 0 && !probe->flags.stopped) {
        sp_probe_range(probe, control.branch + 2, control.body_end, true);
        if (probe->flags.stopped || probe->flags.needs_iteration) {
            return;
        }
        condition = probe_condition(probe, control);
    }
    if (!condition.known) {
        probe->flags.needs_iteration = true;
    }
}

static void probe_if(probe_t *probe, rpn_control_t control, comptime_value_t condition) {
    if (condition.known) {
        if (condition.number != 0) {
            sp_probe_range(probe, control.branch + 2, control.body_end, true);
        } else if (control.has_else) {
            sp_probe_range(probe, control.false_start, control.end, true);
        }
        return;
    }
    bool previous_control = probe->flags.unknown_control;
    probe->flags.unknown_control = true;
    comptime_environment_t original = comptime_copy_environment(&probe->environment);
    sp_probe_range(probe, control.branch + 2, control.body_end, true);
    bool stopped = probe->flags.stopped;
    comptime_environment_t left = probe->environment;
    probe->environment = original;
    probe->flags.stopped = false;
    if (control.has_else) {
        sp_probe_range(probe, control.false_start, control.end, true);
    }
    probe->flags.stopped &= stopped;
    comptime_environment_assign_owned(&probe->environment,
        comptime_join_environments(&left, &probe->environment));
    comptime_environment_free(&left);
    probe->flags.unknown_control = previous_control;
}

static void probe_control(probe_t *probe, rpn_control_t control) {
    comptime_value_t condition = probe_condition(probe, control);
    if (probe->flags.auditing_types) {
        probe_audit_control(probe, control, condition);
    } else if (control.loop) {
        probe_loop(probe, control, condition);
    } else {
        probe_if(probe, control, condition);
    }
}

void sp_probe_range(probe_t *probe, size_t begin, size_t end, bool inspect_first) {
    if (probe->controls == NULL) {
        probe->controls = rpn_index_controls(probe->input);
    }
    semantic_origin_t previous = set_semantic_origin((semantic_origin_t){probe->input, begin});
    for (size_t pc = begin;
         pc < end && !probe->flags.stopped && (probe->flags.auditing_types || !probe->flags.needs_iteration);
         pc++) {
        set_semantic_origin((semantic_origin_t){probe->input, pc});
        size_t branch = (pc != begin || inspect_first) && probe->controls[pc] != 0 ? probe->controls[pc] - 1 : end;
        if (branch < end) {
            rpn_control_t control = rpn_describe_control(probe->input, pc, branch);
            probe_control(probe, control);
            pc = control.end - 1;
        } else {
            probe_step(probe, pc);
        }
    }
    set_semantic_origin(previous);
}

bool sp_requires_iteration(specialize_state_t *state, const rpn_t *input, size_t begin, size_t end,
                           const comptime_environment_t *environment, map_node_t *receiver) {
    for (size_t i = 0; i < state->templates.count; i++) {
        assert(!state->templates.items[i]->exploring);
    }
    probe_t probe = {
        .state = state,
        .input = input,
        .environment = comptime_copy_environment(environment),
        .receiver = receiver
    };
    comptime_forget_runtime(&probe.environment);
    sp_probe_range(&probe, begin, end, true);
    bool result = probe.flags.needs_iteration;
    probe_destroy(&probe);
    return result;
}

comptime_value_t sp_evaluate_call(specialize_state_t *state, function_template_t *source,
                                  comptime_environment_t *environment,
                                  const comptime_environment_t *arguments,
                                  map_node_t *receiver, bool unknown_entry, int line) {
    if (source == NULL || source->active_instance != NULL || source->evaluating) {
        return (comptime_value_t){0};
    }
    source->evaluating = true;
    probe_t probe = {
        .state = state,
        .input = source->body,
        .receiver = receiver,
        .environment = comptime_call_environment(environment, arguments, source->symbol),
        .flags = {.validating = true, .external_unknown = unknown_entry},
        .function = source->symbol
    };
    sp_comptime_arguments_known(source, arguments, line, true);
    sp_check_discarded(state, source->body, source->symbol, 0, source->body->count,
                       &probe.environment, receiver);
    sp_probe_range(&probe, 0, source->body->count, true);
    sp_apply_effects(source, &probe.environment, receiver, environment, receiver);
    comptime_value_t returned = probe.flags.needs_iteration ? (comptime_value_t){0} : probe.returns.value;
    probe_destroy(&probe);
    source->evaluating = false;
    return returned;
}

bool sp_check_discarded(specialize_state_t *state, const rpn_t *body, map_node_t *function,
                        size_t begin, size_t end, const comptime_environment_t *environment,
                        map_node_t *receiver) {
    int errors = global_errors;
    probe_t probe = {
        .state = state,
        .input = body,
        .resolved = {.count = body->count, .capacity = body->count},
        .environment = comptime_copy_environment(environment),
        .receiver = receiver,
        .flags = {.auditing_types = true},
        .function = function
    };
    probe.resolved.steps = memory_zero(body->count + 1, sizeof(*body->steps));
    memcpy(probe.resolved.steps, body->steps, body->count * sizeof(*body->steps));
    sp_probe_range(&probe, begin, end, true);
    if (errors == global_errors) {
        types_check_slice(&probe.resolved, function, body, begin, end);
    }
    probe_destroy(&probe);
    return errors == global_errors;
}
