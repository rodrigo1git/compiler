#include "../include/diagnostics.h"
#include "../include/types.h"
#include "../include/classes.h"
#include "../include/memory.h"
#include <stdlib.h>
#include "../include/specialize_internal.h"
#include "../include/comptime_objects.h"

/* This evaluator works on private environments. It never publishes executable
   instances, output instructions or diagnostics from a discarded attempt. */
typedef struct {
    const map_node_t *symbol;
    rpn_step_kind_t kind;
    const map_node_t *object;
} probe_destination_t;

typedef struct {
    comptime_value_t value;
    probe_destination_t destination;
    comptime_environment_t fields;
} probe_operand_t;

typedef struct probe_call {
    function_template_t *source;
    const map_node_t *receiver;
    comptime_environment_t arguments;
    struct probe_call *parent;
} probe_call_t;

typedef struct {
    const specialize_state_t *state;
    const rpn_t *input;
    size_t *controls;
    comptime_environment_t environment;
    const map_node_t *receiver;
    probe_operand_t *stack;
    size_t count;
    probe_call_t *pending;
    size_t *budget;
    unsigned int depth;
    bool needs_iteration;
    bool validating;
    bool unknown_control;
    bool external_unknown;
    const map_node_t *function;
    int line;
    bool stopped;
    bool has_return;
    comptime_value_t returned;
} probe_t;

static void probe_range(probe_t *probe, size_t begin, size_t end, bool inspect_first);

static void probe_validate_object(probe_t *probe, comptime_value_t value);

static void probe_push(probe_t *probe, probe_operand_t value) {
    if (probe->validating) probe_validate_object(probe, value.value);
    probe->stack = memory_resize(probe->stack, (probe->count + 1) * sizeof(*probe->stack));
    if (value.value.object_type != NULL) value.fields = comptime_copy_environment(&probe->environment);
    probe->stack[probe->count++] = value;
}

static probe_operand_t probe_pop(probe_t *probe) {
    return probe->count ? probe->stack[--probe->count] : (probe_operand_t){0};
}

static void probe_destroy(probe_t *probe) {
    for (size_t i = 0; i < probe->count; i++) free(probe->stack[i].fields.items);
    while (probe->pending != NULL) {
        probe_call_t *parent = probe->pending->parent;
        free(probe->pending->arguments.items);
        free(probe->pending);
        probe->pending = parent;
    }
    free(probe->controls);
    free(probe->stack);
    free(probe->environment.items);
}

static void probe_copy_object(probe_t *probe, comptime_environment_t *destination, const map_node_t *object,
                              const map_node_t *type, comptime_value_t value,
                              const comptime_environment_t *fields, bool argument, unsigned int depth) {
    if (type == NULL || depth == SP_MAX_DEPTH || !*probe->budget) return;
    (*probe->budget)--;
    for (size_t i = 0; i < type->class_info->effective_attributes.count; i++) {
        const map_node_t *field = type->class_info->effective_attributes.items[i];
        comptime_value_t selected = {0};
        if (value.object_type && i < value.object_type->class_info->effective_attributes.count)
            selected = comptime_object_field(value, value.object_type->class_info->effective_attributes.items[i], fields);
        if (field->type_symbol) {
            const map_node_t *child = comptime_object_location(object, field);
            probe_copy_object(probe, destination, child, field->type_symbol, selected, fields, argument,
                              depth + 1);
            selected.object = child;
            selected.object_type = field->type_symbol;
            selected.object_snapshot = NULL;
        } else if (field->is_comptime && !selected.known) {
            probe->needs_iteration = true;
            if (probe->validating)
                report_semantic_error(probe->line, "Comptime attribute '%s' has no known value when copying an object '%s'",
                                      field->key, object ? object->key : type->key);
        }
        if (probe->validating && field->is_comptime && !argument &&
            (probe->unknown_control || (probe->external_unknown && object->owner != probe->function)))
            report_semantic_error(probe->line, "Cannot modify comptime '%s' under unknown control in object '%s'", field->key, object->key);
        if (!argument || field->is_comptime) comptime_set(destination, field, object, selected);
    }
}

static void probe_validate_object(probe_t *probe, comptime_value_t value) {
    if (!class_has_comptime(value.object_type)) return;
    comptime_environment_t capture = {0};
    /* A read captures fields; it is not a write under the enclosing control. */
    probe_copy_object(probe, &capture, value.object, value.object_type, value,
                      &probe->environment, true, 0);
    free(capture.items);
}

static comptime_value_t probe_invoke(probe_t *probe, probe_call_t *call) {
    function_template_t *source = call->source;
    if (source == NULL) return (comptime_value_t){0};
    if (probe->validating)
        return specialize_evaluate_call(source->symbol, &probe->environment, &call->arguments,
                                        call->receiver, probe->unknown_control || probe->external_unknown, probe->line);
    for (size_t i = 0; i < source->symbol->function_info->parameters.count; i++) {
        const map_node_t *parameter = source->symbol->function_info->parameters.items[i];
        if (parameter->is_comptime && !comptime_get(&call->arguments, parameter, NULL).known)
            probe->needs_iteration = true;
    }
    if (probe->needs_iteration || probe->depth == SP_MAX_DEPTH || !*probe->budget) {
        sp_forget_call_writes(&probe->environment, source);
        return (comptime_value_t){0};
    }
    probe_t callee = {.state = probe->state,
                      .input = source->body,
                      .receiver = call->receiver,
                      .environment = comptime_call_environment(&probe->environment, &call->arguments, source->symbol),
                      .depth = probe->depth + 1,
                      .budget = probe->budget};
    probe_range(&callee, 0, source->body->count, true);
    probe->needs_iteration |= callee.needs_iteration;
    sp_apply_effects(source, &callee.environment, call->receiver, &probe->environment, call->receiver);
    comptime_value_t returned = callee.returned;
    probe_destroy(&callee);
    return returned;
}

static void probe_assign(probe_t *probe, probe_destination_t destination, const probe_operand_t *assigned) {
    bool argument = destination.kind == RPN_ARGUMENT_DESTINATION && probe->pending;
    comptime_environment_t *environment = argument ? &probe->pending->arguments : &probe->environment;
    if (destination.symbol) {
        if (probe->validating && destination.symbol->is_comptime)
            comptime_validate_assignment(destination.symbol, assigned->value,
                !argument && destination.symbol->usage != SYMBOL_AUXILIARY &&
                (probe->unknown_control || (probe->external_unknown &&
                destination.symbol->owner != probe->function &&
                (!destination.object || destination.object->owner != probe->function))),
                probe->line);
        if (destination.symbol->type_symbol) {
            const map_node_t *object = destination.object
                                           ? comptime_object_location(destination.object, destination.symbol)
                                           : destination.symbol;
            /* Keep the source snapshot separate, including self-copies. */
            probe_copy_object(probe, environment, object, destination.symbol->type_symbol, assigned->value,
                              &assigned->fields, argument, 0);
        }
        comptime_set(environment, destination.symbol, destination.object, assigned->value);
    } else if (!argument) {
        for (size_t i = 0; i < environment->count; i++)
            if (environment->items[i].object == destination.object)
                environment->items[i].value = (comptime_value_t){0};
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
        free(left.fields.items);
    } else if (op == RPN_TO_INTEGER) {
        operand.value = comptime_to_integer(right.value);
        probe_push(probe, operand);
    } else if (op == RPN_DUPLICATE) {
        operand.value = right.value;
        probe_push(probe, operand);
        probe_push(probe, operand);
    } else if (op == RPN_RETURN) {
        if (!probe->has_return) probe->returned = right.value;
        else if (!comptime_values_equal(probe->returned, right.value))
            probe->returned = (comptime_value_t){0};
        probe->has_return = true;
        probe->stopped = true;
    }
    free(right.fields.items);
}

static void probe_step(probe_t *probe, const rpn_step_t *step) {
    probe_operand_t operand = {0};
    probe->line = step->source_line;
    switch (step->kind) {
    case RPN_SYMBOL:
        operand.value = comptime_read_symbol(&probe->environment, step->value.symbol, probe->receiver);
        if (probe->validating && step->value.symbol->is_comptime && !operand.value.known)
            report_semantic_error(step->source_line, "Comptime '%s' has no known value", step->value.symbol->key);
        operand.value.object_type = step->value.symbol->type_symbol;
        if (operand.value.object_type)
            operand.value.object = comptime_resolve_object(step->value.symbol, probe->receiver);
        probe_push(probe, operand);
        break;
    case RPN_DESTINATION:
    case RPN_ARGUMENT_DESTINATION:
        operand.destination = (probe_destination_t){
            .symbol = step->value.symbol,
            .kind = step->kind,
            .object = step->value.symbol->usage == SYMBOL_ATTRIBUTE ? probe->receiver : NULL};
        probe_push(probe, operand);
        break;
    case RPN_ATTRIBUTE:
    case RPN_ATTRIBUTE_DESTINATION: {
        const map_node_t *object = comptime_resolve_object(step->receiver, probe->receiver);
        comptime_value_t index = comptime_read_symbol(&probe->environment, step->index, probe->receiver);
        const map_node_t *field = comptime_select_field(object ? object->type_symbol : NULL, index);
        if (step->index->is_comptime && !index.known) probe->needs_iteration = true;
        if (probe->validating && step->kind == RPN_ATTRIBUTE &&
            !step->index->is_comptime && step->index->usage != SYMBOL_LEXEME)
            probe_validate_object(probe, (comptime_value_t){.object = object,
                .object_type = object ? object->type_symbol : NULL});
        if (step->kind == RPN_ATTRIBUTE_DESTINATION)
            operand.destination =
                (probe_destination_t){.symbol = field, .object = object, .kind = step->kind};
        else if (field)
            operand.value = comptime_object_field(
                (comptime_value_t){.object = object, .object_type = object->type_symbol}, field,
                &probe->environment);
        if (probe->validating && step->kind == RPN_ATTRIBUTE && field &&
            field->is_comptime && !operand.value.known)
            report_semantic_error(step->source_line, "Comptime attribute '%s' has no known value", field->key);
        probe_push(probe, operand);
        break;
    }
    case RPN_CALL_FRAME: {
        probe_call_t *call = memory_resize(NULL, sizeof(*call));
        *call = (probe_call_t){.source = sp_find_template(probe->state, step->value.symbol),
                               .receiver = comptime_resolve_object(step->receiver, probe->receiver),
                               .parent = probe->pending};
        probe->pending = call;
        break;
    }
    case RPN_CALL: {
        probe_call_t *call = probe->pending;
        if (call == NULL) break;
        operand.value = probe_invoke(probe, call);
        probe->pending = call->parent;
        free(call->arguments.items);
        free(call);
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
    probe_range(probe, control.start, control.branch, false);
    probe_operand_t condition = probe_pop(probe);
    free(condition.fields.items);
    return condition.value;
}

static void probe_loop(probe_t *probe, rpn_control_t control, comptime_value_t condition) {
    if (condition.known && condition.number == 0) return;
    probe_t attempt = {.state = probe->state,
                       .input = probe->input,
                       .receiver = probe->receiver,
                       .environment = comptime_copy_environment(&probe->environment),
                       .depth = probe->depth,
                       .budget = probe->budget};
    comptime_forget_runtime(&attempt.environment);
    probe_range(&attempt, control.branch + 2, control.body_end, true);
    bool needs_iteration = attempt.needs_iteration;
    bool expand = needs_iteration ||
                  sp_has_comptime_effects(probe->state, probe->input, control.branch + 2, control.body_end);
    probe_destroy(&attempt);
    if (!expand || !condition.known) {
        if (probe->validating) {
            bool previous = probe->unknown_control;
            probe->unknown_control = true;
            comptime_forget_runtime(&probe->environment);
            probe_range(probe, control.branch + 2, control.body_end, true);
            probe->unknown_control = previous;
            probe->stopped = false;
        }
        probe->needs_iteration |= needs_iteration;
        comptime_forget_runtime(&probe->environment);
        return;
    }
    while (condition.known && condition.number != 0 && !probe->stopped && *probe->budget) {
        probe_range(probe, control.branch + 2, control.body_end, true);
        if (probe->stopped || probe->needs_iteration) return;
        condition = probe_condition(probe, control);
    }
    if (!condition.known || !*probe->budget) probe->needs_iteration = true;
    return;
}

static void probe_if(probe_t *probe, rpn_control_t control, comptime_value_t condition) {
    if (condition.known) {
        if (condition.number != 0) probe_range(probe, control.branch + 2, control.body_end, true);
        else if (control.has_else) probe_range(probe, control.false_start, control.end, true);
        return;
    }
    bool previous_control = probe->unknown_control;
    probe->unknown_control = true;
    comptime_environment_t original = comptime_copy_environment(&probe->environment);
    probe_range(probe, control.branch + 2, control.body_end, true);
    bool stopped = probe->stopped;
    comptime_environment_t left = probe->environment;
    probe->environment = original;
    probe->stopped = false;
    if (control.has_else) probe_range(probe, control.false_start, control.end, true);
    probe->stopped &= stopped;
    comptime_environment_t joined = comptime_join_environments(&left, &probe->environment);
    free(left.items);
    free(probe->environment.items);
    probe->environment = joined;
    probe->unknown_control = previous_control;
}

static void probe_control(probe_t *probe, rpn_control_t control) {
    comptime_value_t condition = probe_condition(probe, control);
    if (control.loop) probe_loop(probe, control, condition);
    else probe_if(probe, control, condition);
}

static void probe_range(probe_t *probe, size_t begin, size_t end, bool inspect_first) {
    if (!probe->controls) probe->controls = rpn_index_controls(probe->input);
    for (size_t pc = begin; pc < end && !probe->stopped && !probe->needs_iteration; pc++) {
        if (!*probe->budget) break;
        (*probe->budget)--;
        size_t branch = (pc != begin || inspect_first) && probe->controls[pc] ? probe->controls[pc] - 1 : end;
        if (branch < end) {
            rpn_control_t control = rpn_describe_control(probe->input, pc, branch);
            probe_control(probe, control);
            pc = control.end - 1;
        } else probe_step(probe, &probe->input->steps[pc]);
    }
}

bool sp_requires_iteration(const specialize_state_t *state, const rpn_t *input, size_t begin, size_t end,
                           const comptime_environment_t *environment, const map_node_t *receiver) {
    size_t budget = SP_EVALUATION_BUDGET;
    probe_t probe = {.state = state,
                     .input = input,
                     .environment = comptime_copy_environment(environment),
                     .receiver = receiver,
                     .budget = &budget};
    comptime_forget_runtime(&probe.environment);
    probe_range(&probe, begin, end, true);
    bool result = probe.needs_iteration;
    probe_destroy(&probe);
    return result;
}

comptime_value_t sp_evaluate_call(const specialize_state_t *state, function_template_t *source,
    comptime_environment_t *environment, const comptime_environment_t *arguments,
    const map_node_t *receiver, bool unknown_entry, int line) {
    size_t budget = SP_EVALUATION_BUDGET;
    probe_t probe = {.state = state, .input = source->body, .receiver = receiver,
        .environment = comptime_call_environment(environment, arguments, source->symbol),
        .budget = &budget, .validating = true, .function = source->symbol,
        .external_unknown = unknown_entry};
    for (size_t i = 0; i < source->symbol->function_info->parameters.count; i++) {
        const map_node_t *parameter = source->symbol->function_info->parameters.items[i];
        if (parameter->is_comptime && !comptime_get(arguments, parameter, NULL).known)
            report_semantic_error(line, "Argument for comptime parameter '%s' is not known at compile time", parameter->key);
    }
    types_check_discarded(source->body, source->symbol, 0, source->body->count,
                         &probe.environment, receiver);
    probe_range(&probe, 0, source->body->count, true);
    if (!budget) report_semantic_error(line, "Compile-time evaluation exceeded the resource limit");
    sp_apply_effects(source, &probe.environment, receiver, environment, receiver);
    comptime_value_t returned = probe.needs_iteration ? (comptime_value_t){0} : probe.returned;
    probe_destroy(&probe);
    return returned;
}
