#include "../include/memory.h"
#include <stdlib.h>
#include <string.h>
#include "../include/specialize_internal.h"
#include "../include/comptime_objects.h"
#include "../include/diagnostics.h"
#include "../include/classes.h"
#include "../include/types.h"

typedef struct {
    const map_node_t *symbol;
    rpn_step_kind_t kind;
    const map_node_t *object;
    const map_node_t *receiver;
    const map_node_t *index;
} destination_t;

typedef struct {
    comptime_value_t value;
    destination_t destination;
    size_t start;
    size_t end;
    bool removable;
} operand_t;

typedef struct pending_call {
    function_template_t *source;
    const map_node_t *receiver;
    comptime_environment_t arguments;
    size_t frame_step;
    int errors;
    struct pending_call *parent;
} pending_call_t;

typedef struct {
    specialize_state_t *state;
    const rpn_t *input;
    rpn_t *output;
    comptime_environment_t *environment;
    map_node_t *function;
    const map_node_t *receiver;
    operand_t *stack;
    size_t count;
    pending_call_t *pending;
    bool external_unknown;
    bool stopped;
    bool has_return;
    comptime_value_t returned;
    size_t *controls;
} context_t;

static void transform_range(context_t *context, size_t begin, size_t end, bool unknown_control,
                            bool inspect_first_control);

static void push(context_t *context, operand_t operand) {
    context->stack = memory_resize(context->stack, (context->count + 1) * sizeof(*context->stack));
    context->stack[context->count++] = operand;
}

static operand_t pop(context_t *context, int line) {
    if (context->count != 0) return context->stack[--context->count];
    report_semantic_error(line, "Invalid intermediate expression stack");
    return (operand_t){0};
}

static size_t emit(context_t *context, rpn_step_t step) {
    return rpn_append_step(context->output, step);
}

static void discard(context_t *context, operand_t operand, int line) {
    if (operand.removable && operand.end == context->output->count) context->output->count = operand.start;
    else emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_DROP, .source_line = line});
}

static void materialize_object(context_t *context, operand_t *operand, int line) {
    map_node_t *snapshot =
        comptime_capture_object(operand->value, context->environment, &context->state->serial, line);
    if (snapshot == NULL) return;
    emit(context, (rpn_step_t){.kind = RPN_OBJECT_VALUE, .value.symbol = snapshot, .source_line = line});
    operand->value.object_snapshot = snapshot;
}

static void record_return(context_t *context, comptime_value_t value) {
    if (!context->has_return) context->returned = value;
    else if (!comptime_values_equal(context->returned, value)) context->returned = (comptime_value_t){0};
    context->has_return = true;
}

static operand_t process_condition(context_t *context, rpn_control_t control, bool unknown_control) {
    transform_range(context, control.start, control.branch, unknown_control, false);
    return pop(context, context->input->steps[control.branch].source_line);
}

static void check_discarded(context_t *context, size_t begin, size_t end) {
    if (begin < end)
        types_check_discarded(context->input, context->function, begin, end, context->environment,
                              context->receiver);
}

static void process_if(context_t *context, rpn_control_t control, bool unknown_control) {
    int line = context->input->steps[control.branch].source_line;
    size_t condition_output = context->output->count;
    operand_t condition = process_condition(context, control, unknown_control);
    if (condition.value.known) {
        discard(context, condition, line);
        if (condition.value.number != 0 && control.has_else)
            check_discarded(context, control.false_start, control.end);
        else if (condition.value.number == 0) check_discarded(context, control.branch + 2, control.body_end);
        if (condition.value.number != 0)
            transform_range(context, control.branch + 2, control.body_end, unknown_control, true);
        else if (control.has_else)
            transform_range(context, control.false_start, control.end, unknown_control, true);
        return;
    }
    size_t false_target = rpn_reserve_target(context->output);
    context->output->steps[false_target - 1].control_start = condition_output + 1;
    emit(context,
         (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_BRANCH_FALSE, .source_line = line});
    comptime_environment_t original = comptime_copy_environment(context->environment);
    transform_range(context, control.branch + 2, control.body_end, true, true);
    bool true_stopped = context->stopped;
    comptime_environment_t true_environment = comptime_copy_environment(context->environment);
    context->stopped = false;
    if (control.has_else) {
        size_t end_target = rpn_reserve_target(context->output);
        emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_BRANCH, .source_line = line});
        rpn_patch_target(context->output, false_target, context->output->count + 1);
        free(context->environment->items);
        *context->environment = comptime_copy_environment(&original);
        transform_range(context, control.false_start, control.end, true, true);
        context->stopped = true_stopped && context->stopped;
        rpn_patch_target(context->output, end_target, context->output->count + 1);
    } else rpn_patch_target(context->output, false_target, context->output->count + 1);
    comptime_environment_t joined =
        comptime_join_environments(&true_environment, control.has_else ? context->environment : &original);
    free(context->environment->items);
    free(original.items);
    free(true_environment.items);
    *context->environment = joined;
}

static void process_loop(context_t *context, rpn_control_t control, bool unknown_control) {
    int line = context->input->steps[control.branch].source_line;
    bool effects =
        sp_has_comptime_effects(context->state, context->input, control.branch + 2, control.body_end);
    bool expand = effects;
    size_t condition_output = context->output->count;
    operand_t condition = process_condition(context, control, unknown_control);
    if (condition.value.known && condition.value.number == 0) {
        check_discarded(context, control.branch + 2, control.body_end);
        discard(context, condition, line);
        return;
    }
    if (!expand && condition.value.known)
        expand = sp_requires_iteration(context->state, context->input, control.branch + 2, control.body_end,
                                       context->environment, context->receiver);
    if (expand && condition.value.known && (!unknown_control || !effects)) {
        for (;;) {
            discard(context, condition, line);
            if (condition.value.number == 0 || context->stopped) break;
            if (context->state->evaluation_budget-- == 0) {
                report_semantic_error(line, "Compile-time loop evaluation exceeded the resource limit");
                break;
            }
            int errors = global_errors;
            transform_range(context, control.branch + 2, control.body_end, unknown_control, true);
            if (context->stopped || global_errors != errors) break;
            condition = process_condition(context, control, unknown_control);
            if (!condition.value.known) {
                report_semantic_error(line, "Compile-time loop control became unknown");
                transform_range(context, control.branch + 2, control.body_end, true, true);
                break;
            }
        }
        return;
    }
    size_t false_target = rpn_reserve_target(context->output);
    context->output->steps[false_target - 1].control_start = condition_output + 1;
    context->output->steps[false_target - 1].loop = true;
    context->output->steps[false_target - 1].loop_variable =
        context->input->steps[control.branch].loop_variable;
    emit(context,
         (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_BRANCH_FALSE, .source_line = line});
    comptime_environment_t original = comptime_copy_environment(context->environment);
    comptime_forget_runtime(context->environment);
    transform_range(context, control.branch + 2, control.body_end, true, true);
    context->stopped = false;
    size_t back = rpn_reserve_target(context->output);
    emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_BRANCH, .source_line = line});
    rpn_patch_target(context->output, back, condition_output + 1);
    rpn_patch_target(context->output, false_target, context->output->count + 1);
    free(context->environment->items);
    *context->environment = original;
    comptime_forget_runtime(context->environment);
}

static size_t transform_control(context_t *context, size_t start, size_t branch, bool unknown_control) {
    rpn_control_t control = rpn_describe_control(context->input, start, branch);
    if (control.loop) process_loop(context, control, unknown_control);
    else process_if(context, control, unknown_control);
    return control.end;
}

static void evaluate_binary(context_t *context, rpn_step_t step) {
    operand_t right = pop(context, step.source_line), left = pop(context, step.source_line);
    operand_t result = {.value = comptime_binary(left.value, right.value, step.value.operator)};
    result.start = left.start;
    result.removable =
        left.removable && right.removable && left.end == right.start && right.end == context->output->count;
    emit(context, step);
    result.end = context->output->count;
    push(context, result);
}

static void assign_comptime(context_t *context, destination_t destination, comptime_value_t value,
                            bool unknown_control, int line) {
    const map_node_t *symbol = destination.symbol;
    bool argument = destination.kind == RPN_ARGUMENT_DESTINATION;
    bool local = symbol->owner == context->function ||
                 (destination.object != NULL && destination.object->owner == context->function);
    bool forbidden = !argument && symbol->usage != SYMBOL_AUXILIARY &&
                     (unknown_control || (context->external_unknown && !local));
    if (comptime_validate_assignment(symbol, value, forbidden, line)) {
        const map_node_t *constant = comptime_make_constant(value, line);
        if (constant != NULL) {
            value.constant = constant;
            if (argument && context->pending != NULL)
                comptime_set(&context->pending->arguments, symbol, NULL, value);
            else comptime_set(context->environment, symbol, destination.object, value);
        }
    }
}

static void assign_value(context_t *context, destination_t destination, operand_t assigned,
                         bool unknown_control, int line) {
    comptime_value_t value = assigned.value;
    const map_node_t *symbol = destination.symbol;
    bool argument = destination.kind == RPN_ARGUMENT_DESTINATION;
    if (symbol != NULL && symbol->is_comptime) {
        assign_comptime(context, destination, value, unknown_control, line);
        discard(context, assigned, line);
        return;
    }
    rpn_step_t step = {.kind = destination.kind,
                       .value.symbol = symbol,
                       .source_line = line,
                       .receiver = destination.receiver,
                       .index = destination.index};
    emit(context, step);
    emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_ASSIGN, .source_line = line});
    if (argument && symbol != NULL && symbol->type_symbol != NULL && context->pending != NULL) {
        comptime_copy_t copy = {.source = context->environment,
                                .destination = &context->pending->arguments,
                                .function = context->function,
                                .argument = true,
                                .external_unknown = context->external_unknown};
        comptime_copy_object_fields(&copy, symbol, symbol->type_symbol, value, line);
    }
    if (symbol != NULL) class_check_copy(symbol->type_symbol, value.object_type, line);
    if (!argument && symbol != NULL && symbol->type_symbol != NULL) {
        const map_node_t *object =
            destination.kind == RPN_ATTRIBUTE_DESTINATION || symbol->usage == SYMBOL_ATTRIBUTE
                ? comptime_object_location(destination.object, symbol)
                : symbol;
        comptime_copy_t copy = {.source = context->environment,
                                .destination = context->environment,
                                .function = context->function,
                                .unknown_control = unknown_control,
                                .external_unknown = context->external_unknown};
        comptime_copy_object_fields(&copy, object, symbol->type_symbol, value, line);
        value.object = object;
        value.object_type = symbol->type_symbol;
        value.object_snapshot = NULL;
    }
    if (!argument && destination.kind == RPN_ATTRIBUTE_DESTINATION && symbol == NULL)
        for (size_t i = 0; i < context->environment->count; i++)
            if (context->environment->items[i].object == destination.object)
                context->environment->items[i].value = (comptime_value_t){0};
    if (symbol != NULL && !argument) comptime_set(context->environment, symbol, destination.object, value);
}

static void process_symbol(context_t *context, rpn_step_t step) {
    operand_t operand = {.start = context->output->count, .removable = true};
    const map_node_t *symbol = step.value.symbol;
    operand.value = comptime_read_symbol(context->environment, symbol, context->receiver);
    if (symbol->is_comptime) {
        if (!operand.value.known)
            report_semantic_error(step.source_line, "Comptime '%s' has no known value", symbol->key);
        else {
            const map_node_t *constant = comptime_make_constant(operand.value, step.source_line);
            if (constant != NULL) step.value.symbol = constant;
        }
    }
    operand.value.object_type = symbol->type_symbol;
    operand.value.object =
        operand.value.object_type == NULL ? NULL : comptime_resolve_object(symbol, context->receiver);
    operand.value.object_snapshot = NULL;
    operand.start = context->output->count;
    operand.removable = true;
    emit(context, step);
    materialize_object(context, &operand, step.source_line);
    operand.end = context->output->count;
    push(context, operand);
}

typedef struct {
    const map_node_t *object;
    map_node_t *field;
    bool runtime_index;
} attribute_access_t;

static attribute_access_t resolve_attribute(context_t *context, rpn_step_t *step) {
    const map_node_t *object = comptime_resolve_object(step->receiver, context->receiver);
    const map_node_t *index = step->index;
    comptime_value_t selected = comptime_read_symbol(context->environment, index, context->receiver);
    if (index->is_comptime) {
        if (!selected.known)
            report_semantic_error(step->source_line, "Comptime index '%s' has no known value", index->key);
        else step->index = comptime_make_constant(selected, step->source_line);
    }
    bool runtime_index = !index->is_comptime && index->usage != SYMBOL_LEXEME;
    map_node_t *attribute = (map_node_t *)comptime_select_field(object->type_symbol, selected);
    if (runtime_index && selected.known &&
        (selected.number < 0 || selected.number >= object->type_symbol->class_info->effective_attributes.count))
        selected.known = false;
    if (selected.known && !selected.floating) {
        if (selected.number < 0)
            report_semantic_error(step->source_line, "Attribute position %ld does not exist in class '%s'",
                                  (long)selected.number, object->type_symbol->key);
        else attribute = class_attribute_at(object->type_symbol, (size_t)selected.number, step->source_line);
    }
    if (((attribute == NULL && !selected.known) || runtime_index) &&
        class_has_comptime(object->type_symbol)) {
        if (step->kind == RPN_ATTRIBUTE) {
            comptime_value_t receiver = {.object_type = object->type_symbol, .object = object};
            step->capture = comptime_capture_object(receiver, context->environment, &context->state->serial,
                                                    step->source_line);
        } else
            report_semantic_error(step->source_line,
                                  "Attribute position must be known when accessing comptime attributes");
    }
    return (attribute_access_t){object, attribute, runtime_index};
}

static void process_attribute(context_t *context, rpn_step_t step) {
    operand_t operand = {.start = context->output->count, .removable = true};
    attribute_access_t access = resolve_attribute(context, &step);
    const map_node_t *object = access.object;
    map_node_t *attribute = access.field;
    bool runtime_index = access.runtime_index;
    if (step.kind == RPN_ATTRIBUTE_DESTINATION) {
        operand.destination.symbol = attribute;
        operand.destination.kind = step.kind;
        operand.destination.index = step.index;
        operand.destination.object = object;
        operand.destination.receiver = step.receiver;
        push(context, operand);
    } else {
        if (attribute != NULL) operand.value = comptime_get(context->environment, attribute, object);
        if (attribute != NULL && attribute->is_comptime && !runtime_index) {
            if (!operand.value.known)
                report_semantic_error(step.source_line, "Comptime attribute '%s' has no known value",
                                      attribute->key);
            else {
                step.kind = RPN_SYMBOL;
                step.value.symbol = comptime_make_constant(operand.value, step.source_line);
            }
        }
        operand.value.object_type = attribute != NULL ? attribute->type_symbol : NULL;
        operand.value.object =
            operand.value.object_type != NULL ? comptime_object_location(object, attribute) : NULL;
        operand.value.object_snapshot = NULL;
        operand.start = context->output->count;
        operand.removable = true;
        emit(context, step);
        materialize_object(context, &operand, step.source_line);
        operand.end = context->output->count;
        push(context, operand);
    }
}

static void begin_call(context_t *context, rpn_step_t step) {
    pending_call_t *pending = memory_resize(NULL, sizeof(*pending));
    *pending = (pending_call_t){.source = sp_find_template(context->state, step.value.symbol),
                                .receiver = comptime_resolve_object(step.receiver, context->receiver),
                                .frame_step = context->output->count,
                                .errors = global_errors,
                                .parent = context->pending};
    context->pending = pending;
    emit(context, step);
}

static void process_call(context_t *context, rpn_step_t step, bool unknown_control) {
    operand_t operand = {.start = context->output->count, .removable = true};
    pending_call_t *pending = context->pending;
    if (pending == NULL || pending->source == NULL) {
        report_semantic_error(step.source_line, "Cannot resolve intermediate call");
        return;
    }
    function_instance_t *instance = sp_find_active_instance(context->state, pending->source);
    if (!instance && global_errors == pending->errors)
        instance =
            sp_instantiate(context->state, pending->source, context->environment, &pending->arguments,
                           pending->receiver, unknown_control || context->external_unknown, step.source_line);
    if (instance != NULL) {
        context->output->steps[pending->frame_step].value.symbol = instance->symbol;
        step.value.symbol = instance->symbol;
        if (!instance->active) {
            operand.value = instance->returned;
            sp_apply_effects(instance->source, &instance->after, instance->receiver, context->environment,
                             pending->receiver);
        } else sp_forget_call_writes(context->environment, pending->source);
    }
    emit(context, step);
    operand.start = pending->frame_step;
    operand.end = context->output->count;
    operand.removable = false;
    push(context, operand);
    if (instance == NULL) sp_forget_call_writes(context->environment, pending->source);
    context->pending = pending->parent;
    free(pending->arguments.items);
    free(pending);
}

static void process_operator(context_t *context, rpn_step_t step, bool unknown_control) {
    operand_t operand = {.start = context->output->count, .removable = true};
    if (step.value.operator <= RPN_NOT_EQUAL && step.value.operator != RPN_ASSIGN)
        evaluate_binary(context, step);
    else if (step.value.operator == RPN_ASSIGN) {
        operand_t destination = pop(context, step.source_line);
        operand_t assigned = pop(context, step.source_line);
        assign_value(context, destination.destination, assigned, unknown_control, step.source_line);
    } else if (step.value.operator == RPN_DUPLICATE) {
        operand = pop(context, step.source_line);
        push(context, operand);
        operand.start = context->output->count;
        emit(context, step);
        operand.end = context->output->count;
        operand.removable = true;
        push(context, operand);
    } else if (step.value.operator == RPN_TO_INTEGER) {
        operand = pop(context, step.source_line);
        operand.value = comptime_to_integer(operand.value);
        operand.removable = operand.removable && operand.end == context->output->count;
        emit(context, step);
        operand.end = context->output->count;
        push(context, operand);
    } else if (step.value.operator == RPN_RETURN || step.value.operator == RPN_OUTPUT) {
        operand = pop(context, step.source_line);
        if (step.value.operator == RPN_RETURN) {
            if (operand.value.object_type != NULL)
                report_semantic_error(step.source_line, "Object returns are not allowed");
            record_return(context, operand.value);
            context->stopped = true;
        }
        emit(context, step);
    } else if (step.value.operator == RPN_DROP) {
        pop(context, step.source_line);
        emit(context, step);
    } else report_semantic_error(step.source_line, "Unstructured branch in intermediate code");
}

static void transform_range(context_t *context, size_t begin, size_t end, bool unknown_control,
                            bool inspect_first_control) {
    semantic_origin_t previous_origin = set_semantic_origin((semantic_origin_t){context->input, begin});
    for (size_t pc = begin; pc < end && !context->stopped; pc++) {
        set_semantic_origin((semantic_origin_t){context->input, pc});
        if ((pc != begin || inspect_first_control) && context->controls[pc] != 0) {
            pc = transform_control(context, pc, context->controls[pc] - 1, unknown_control) - 1;
            if (context->stopped) check_discarded(context, pc + 1, end);
            continue;
        }
        rpn_step_t step = context->input->steps[pc];
        operand_t value = {0};
        value.start = context->output->count;
        value.removable = true;
        switch (step.kind) {
        case RPN_SYMBOL:
            process_symbol(context, step);
            break;
        case RPN_DESTINATION:
        case RPN_ARGUMENT_DESTINATION:
            value.destination.symbol = step.value.symbol;
            value.destination.kind = step.kind;
            value.destination.object =
                step.value.symbol->usage == SYMBOL_ATTRIBUTE ? context->receiver : NULL;
            push(context, value);
            break;
        case RPN_ATTRIBUTE:
        case RPN_ATTRIBUTE_DESTINATION:
            process_attribute(context, step);
            break;
        case RPN_INDEX_CHECK:
            if (step.index->is_comptime) resolve_attribute(context, &step);
            else emit(context, step);
            break;
        case RPN_CALL_FRAME:
            begin_call(context, step);
            break;
        case RPN_CALL:
            process_call(context, step, unknown_control);
            break;
        case RPN_OPERATOR:
            process_operator(context, step, unknown_control);
            break;
        case RPN_TARGET:
            report_semantic_error(step.source_line, "Unexpected standalone jump target");
            break;
        case RPN_OBJECT_VALUE:
            report_semantic_error(step.source_line, "Unexpected materialized object in a source template");
            break;
        }
        if (context->stopped) check_discarded(context, pc + 1, end);
    }
    set_semantic_origin(previous_origin);
}

void sp_transform(specialize_state_t *state, const rpn_t *input, rpn_t *output,
                  comptime_environment_t *environment, map_node_t *function, const map_node_t *receiver,
                  bool external_unknown, comptime_value_t *returned) {
    context_t context = {.state = state,
                         .input = input,
                         .output = output,
                         .environment = environment,
                         .function = function,
                         .receiver = receiver,
                         .external_unknown = external_unknown};
    context.controls = rpn_index_controls(input);
    transform_range(&context, 0, input->count, false, true);
    if (context.count != 0)
        report_semantic_error(function != NULL ? function->declaration_line : 1,
                              "Intermediate body leaves an incomplete expression");
    while (context.pending != NULL) {
        pending_call_t *previous = context.pending->parent;
        free(context.pending->arguments.items);
        free(context.pending);
        context.pending = previous;
    }
    if (returned != NULL) *returned = context.returned;
    free(context.controls);
    free(context.stack);
}
