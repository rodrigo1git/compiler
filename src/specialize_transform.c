#include "../include/memory.h"
#include <stdlib.h>
#include <string.h>
#include "../include/specialize_internal.h"
#include "../include/comptime_objects.h"
#include "../include/diagnostics.h"
#include "../include/classes.h"

typedef struct {
    comptime_value_t value;
    sp_destination_t destination;
    size_t start;
    size_t end;
    bool removable;
} operand_t;

typedef struct {
    map_node_t *object;
    map_node_t *field;
    bool runtime_index;
} attribute_access_t;

struct transform_context {
    specialize_state_t *state;
    const rpn_t *input;
    rpn_t *output;
    comptime_environment_t *environment;
    map_node_t *function;
    map_node_t *receiver;
    operand_t *stack;
    size_t count;
    sp_pending_call_t *pending;
    bool external_unknown;
    bool stopped;
    sp_return_state_t returns;
    size_t *controls;
};

static void push(context_t *context, operand_t operand) {
    context->stack = memory_resize(context->stack, (context->count + 1) * sizeof(*context->stack));
    context->stack[context->count++] = operand;
}

static operand_t pop(context_t *context, int line) {
    if (context->count != 0) {
        return context->stack[--context->count];
    }
    report_semantic_error(line, "Invalid intermediate expression stack");
    return (operand_t){0};
}

static size_t emit(context_t *context, rpn_step_t step) {
    return rpn_append_step(context->output, step);
}

static void discard(context_t *context, operand_t operand, int line) {
    if (operand.removable && operand.end == context->output->count) {
        context->output->count = operand.start;
    } else {
        rpn_emit_operator(context->output, RPN_DROP, line);
    }
}

static void materialize_object(context_t *context, operand_t *operand, int line) {
    map_node_t *snapshot =
        comptime_capture_object(operand->value, context->environment, &context->state->serial, line);
    if (snapshot == NULL) {
        return;
    }
    emit(context, (rpn_step_t){.kind = RPN_OBJECT_VALUE, .value.symbol = snapshot, .source_line = line});
    operand->value.object_snapshot = snapshot;
}

static operand_t process_condition(context_t *context, rpn_control_t control, bool unknown_control) {
    sp_range_options_t options = {.unknown_control = unknown_control, .inspect_first = false};
    sp_transform_range(context, control.start, control.branch, options);
    return pop(context, context->input->steps[control.branch].source_line);
}

static void check_discarded(context_t *context, size_t begin, size_t end) {
    if (begin < end) {
        sp_check_discarded(context->state, context->input, context->function, begin, end,
                           context->environment, context->receiver);
    }
}

static void check_dead_branch(context_t *context, rpn_control_t control, bool taken) {
    if (taken && control.has_else) {
        check_discarded(context, control.false_start, control.end);
    } else if (!taken) {
        check_discarded(context, control.branch + 2, control.body_end);
    }
}

static void process_if(context_t *context, rpn_control_t control, bool unknown_control) {
    int line = context->input->steps[control.branch].source_line;
    size_t condition_output = context->output->count;
    operand_t condition = process_condition(context, control, unknown_control);
    if (condition.value.known) {
        bool taken = condition.value.number != 0;
        discard(context, condition, line);
        check_dead_branch(context, control, taken);
        sp_range_options_t branch_options = {.unknown_control = unknown_control, .inspect_first = true};
        if (taken) {
            sp_transform_range(context, control.branch + 2, control.body_end, branch_options);
        } else if (control.has_else) {
            sp_transform_range(context, control.false_start, control.end, branch_options);
        }
        return;
    }
    size_t false_target = rpn_emit_branch_false(context->output, condition_output + 1, NULL, line);
    comptime_environment_t original = comptime_copy_environment(context->environment);
    sp_range_options_t runtime_options = {.unknown_control = true, .inspect_first = true};
    sp_transform_range(context, control.branch + 2, control.body_end, runtime_options);
    bool true_stopped = context->stopped;
    comptime_environment_t true_environment = comptime_copy_environment(context->environment);
    context->stopped = false;
    if (control.has_else) {
        size_t end_target = rpn_emit_branch(context->output, 0, line);
        rpn_patch_to_end(context->output, false_target);
        comptime_environment_assign_owned(context->environment, comptime_copy_environment(&original));
        sp_transform_range(context, control.false_start, control.end, runtime_options);
        context->stopped = true_stopped && context->stopped;
        rpn_patch_to_end(context->output, end_target);
    } else {
        rpn_patch_to_end(context->output, false_target);
    }
    comptime_environment_t joined =
        comptime_join_environments(&true_environment, control.has_else ? context->environment : &original);
    comptime_environment_assign_owned(context->environment, joined);
    comptime_environment_free(&original);
    comptime_environment_free(&true_environment);
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
    if (!expand && condition.value.known) {
        expand = sp_requires_iteration(context->state, context->input, control.branch + 2, control.body_end,
                                       context->environment, context->receiver);
    }
    if (expand && condition.value.known && (!unknown_control || !effects)) {
        sp_range_options_t body_options = {.unknown_control = unknown_control, .inspect_first = true};
        for (;;) {
            discard(context, condition, line);
            if (condition.value.number == 0 || context->stopped) {
                break;
            }
            int errors = global_errors;
            sp_transform_range(context, control.branch + 2, control.body_end, body_options);
            if (context->stopped || global_errors != errors) {
                break;
            }
            condition = process_condition(context, control, unknown_control);
            if (!condition.value.known) {
                report_semantic_error(line, "Compile-time loop control became unknown");
                sp_range_options_t fallback_options = {.unknown_control = true, .inspect_first = true};
                sp_transform_range(context, control.branch + 2, control.body_end, fallback_options);
                break;
            }
        }
        return;
    }
    map_node_t *loop_variable = context->input->steps[control.branch].loop_variable;
    size_t false_target = rpn_emit_branch_false(context->output, condition_output + 1, loop_variable, line);
    comptime_environment_t original = comptime_copy_environment(context->environment);
    comptime_forget_runtime(context->environment);
    sp_range_options_t runtime_options = {.unknown_control = true, .inspect_first = true};
    sp_transform_range(context, control.branch + 2, control.body_end, runtime_options);
    context->stopped = false;
    rpn_emit_branch(context->output, condition_output + 1, line);
    rpn_patch_to_end(context->output, false_target);
    comptime_environment_assign_owned(context->environment, original);
    comptime_forget_runtime(context->environment);
}

static size_t transform_control(context_t *context, size_t start, size_t branch, bool unknown_control) {
    rpn_control_t control = rpn_describe_control(context->input, start, branch);
    if (control.loop) {
        process_loop(context, control, unknown_control);
    } else {
        process_if(context, control, unknown_control);
    }
    return control.end;
}

static void evaluate_binary(context_t *context, rpn_step_t step) {
    operand_t right = pop(context, step.source_line);
    operand_t left = pop(context, step.source_line);
    operand_t result = {.value = comptime_binary(left.value, right.value, step.value.operator)};
    result.start = left.start;
    result.removable =
        left.removable && right.removable && left.end == right.start && right.end == context->output->count;
    emit(context, step);
    result.end = context->output->count;
    push(context, result);
}

static void assign_comptime(context_t *context, sp_destination_t destination, comptime_value_t value,
                            bool unknown_control, int line) {
    map_node_t *symbol = destination.symbol;
    bool argument = destination.kind == RPN_ARGUMENT_DESTINATION;
    bool forbidden = sp_is_forbidden_comptime_write(symbol, destination.object, context->function,
                                                    argument, unknown_control, context->external_unknown);
    if (!comptime_validate_assignment(symbol, value, forbidden, line)) {
        return;
    }
    map_node_t *constant = comptime_make_constant(value, line);
    if (constant == NULL) {
        return;
    }
    value.constant = constant;
    if (argument && context->pending != NULL) {
        comptime_set(&context->pending->arguments, symbol, NULL, value);
    } else {
        comptime_set(context->environment, symbol, destination.object, value);
    }
}

static void emit_runtime_assignment(context_t *context, sp_destination_t destination, int line) {
    rpn_step_t step = {
        .kind = destination.kind,
        .value.symbol = destination.symbol,
        .source_line = line,
        .receiver = destination.receiver,
        .index = destination.index
    };
    emit(context, step);
    rpn_emit_operator(context->output, RPN_ASSIGN, line);
}

static void copy_argument_fields(context_t *context, map_node_t *symbol, comptime_value_t value, int line) {
    if (symbol == NULL || symbol->type_symbol == NULL || context->pending == NULL) {
        return;
    }
    comptime_copy_t copy = {
        .source = context->environment,
        .destination = &context->pending->arguments,
        .function = context->function,
        .argument = true,
        .external_unknown = context->external_unknown
    };
    comptime_copy_object_fields(&copy, symbol, symbol->type_symbol, value, line);
}

static void copy_local_fields(context_t *context, sp_destination_t destination, comptime_value_t *value,
                              bool unknown_control, int line) {
    map_node_t *symbol = destination.symbol;
    if (symbol == NULL || symbol->type_symbol == NULL) {
        return;
    }
    map_node_t *object =
        destination.kind == RPN_ATTRIBUTE_DESTINATION || symbol->usage == SYMBOL_ATTRIBUTE
            ? comptime_object_location(destination.object, symbol)
            : symbol;
    comptime_copy_t copy = {
        .source = context->environment,
        .destination = context->environment,
        .function = context->function,
        .unknown_control = unknown_control,
        .external_unknown = context->external_unknown
    };
    comptime_copy_object_fields(&copy, object, symbol->type_symbol, *value, line);
    value->object = object;
    value->object_type = symbol->type_symbol;
    value->object_snapshot = NULL;
}

static void assign_value(context_t *context, sp_destination_t destination, operand_t assigned,
                         bool unknown_control, int line) {
    comptime_value_t value = assigned.value;
    map_node_t *symbol = destination.symbol;
    bool argument = destination.kind == RPN_ARGUMENT_DESTINATION;
    if (symbol != NULL && symbol->is_comptime) {
        assign_comptime(context, destination, value, unknown_control, line);
        discard(context, assigned, line);
        return;
    }
    emit_runtime_assignment(context, destination, line);
    if (argument) {
        copy_argument_fields(context, symbol, value, line);
    }
    if (symbol != NULL) {
        class_check_copy(symbol->type_symbol, value.object_type, line);
    }
    if (!argument) {
        copy_local_fields(context, destination, &value, unknown_control, line);
        if (destination.kind == RPN_ATTRIBUTE_DESTINATION && symbol == NULL) {
            comptime_invalidate_object(context->environment, destination.object);
        }
        if (symbol != NULL) {
            comptime_set(context->environment, symbol, destination.object, value);
        }
    }
}

static void process_symbol(context_t *context, rpn_step_t step) {
    operand_t operand = {.start = context->output->count, .removable = true};
    map_node_t *symbol = step.value.symbol;
    operand.value = comptime_read_symbol(context->environment, symbol, context->receiver);
    if (symbol->is_comptime) {
        if (!operand.value.known) {
            report_semantic_error(step.source_line, "Comptime '%s' has no known value", symbol->key);
        } else {
            map_node_t *constant = comptime_make_constant(operand.value, step.source_line);
            if (constant != NULL) {
                step.value.symbol = constant;
            }
        }
    }
    operand.value.object_type = symbol->type_symbol;
    operand.value.object =
        operand.value.object_type == NULL ? NULL : comptime_resolve_object(symbol, context->receiver);
    operand.value.object_snapshot = NULL;
    emit(context, step);
    materialize_object(context, &operand, step.source_line);
    operand.end = context->output->count;
    push(context, operand);
}

static comptime_value_t resolve_attribute_index(context_t *context, rpn_step_t *step) {
    map_node_t *index = step->index;
    comptime_value_t selected = comptime_read_symbol(context->environment, index, context->receiver);
    if (index->is_comptime) {
        if (!selected.known) {
            report_semantic_error(step->source_line, "Comptime index '%s' has no known value", index->key);
        } else {
            step->index = comptime_make_constant(selected, step->source_line);
        }
    }
    return selected;
}

static map_node_t *select_attribute_field(map_node_t *type, comptime_value_t *selected,
                                          bool runtime_index, int line) {
    map_node_t *attribute = comptime_select_field(type, *selected);
    if (runtime_index && attribute == NULL) {
        selected->known = false;
    }
    return sp_attribute_at(type, *selected, line);
}

static void capture_dynamic_attribute(context_t *context, rpn_step_t *step, map_node_t *object,
                                      map_node_t *type, const map_node_t *attribute,
                                      comptime_value_t selected, bool runtime_index) {
    if (((attribute == NULL && !selected.known) || runtime_index) &&
        class_has_comptime(type)) {
        if (step->kind == RPN_ATTRIBUTE) {
            comptime_value_t receiver = {.object_type = type, .object = object};
            step->capture = comptime_capture_object(receiver, context->environment, &context->state->serial,
                                                    step->source_line);
        } else {
            report_semantic_error(step->source_line,
                                  "Attribute position must be known when accessing comptime attributes");
        }
    }
}

static attribute_access_t resolve_attribute(context_t *context, rpn_step_t *step) {
    map_node_t *object = comptime_resolve_object(step->receiver, context->receiver);
    map_node_t *type = object != NULL ? object->type_symbol : step->receiver->type_symbol;
    map_node_t *index = step->index;
    comptime_value_t selected = resolve_attribute_index(context, step);
    bool runtime_index = !index->is_comptime && index->usage != SYMBOL_LEXEME;
    map_node_t *attribute = select_attribute_field(type, &selected, runtime_index, step->source_line);
    capture_dynamic_attribute(context, step, object, type, attribute, selected, runtime_index);
    return (attribute_access_t){object, attribute, runtime_index};
}

static void process_attribute(context_t *context, rpn_step_t step) {
    operand_t operand = {.start = context->output->count, .removable = true};
    attribute_access_t access = resolve_attribute(context, &step);
    map_node_t *object = access.object;
    map_node_t *attribute = access.field;
    bool runtime_index = access.runtime_index;
    if (step.kind == RPN_ATTRIBUTE_DESTINATION) {
        operand.destination.symbol = attribute;
        operand.destination.kind = step.kind;
        operand.destination.index = step.index;
        operand.destination.object = object;
        operand.destination.receiver = step.receiver;
        push(context, operand);
        return;
    }
    if (attribute != NULL) {
        operand.value = comptime_get(context->environment, attribute, object);
    }
    if (attribute != NULL && attribute->is_comptime && !runtime_index) {
        if (!operand.value.known) {
            report_semantic_error(step.source_line, "Comptime attribute '%s' has no known value",
                                  attribute->key);
        } else {
            step.kind = RPN_SYMBOL;
            step.value.symbol = comptime_make_constant(operand.value, step.source_line);
        }
    }
    operand.value.object_type = attribute != NULL ? attribute->type_symbol : NULL;
    operand.value.object =
        operand.value.object_type != NULL ? comptime_object_location(object, attribute) : NULL;
    operand.value.object_snapshot = NULL;
    emit(context, step);
    materialize_object(context, &operand, step.source_line);
    operand.end = context->output->count;
    push(context, operand);
}

static void begin_call(context_t *context, rpn_step_t step) {
    sp_pending_push(&context->pending, sp_find_template(context->state, step.value.symbol),
                    comptime_resolve_object(step.receiver, context->receiver),
                    context->output->count, global_errors);
    emit(context, step);
}

static void process_call(context_t *context, rpn_step_t step, bool unknown_control) {
    operand_t operand = {.start = context->output->count, .removable = true};
    sp_pending_call_t *pending = context->pending;
    if (pending == NULL || pending->source == NULL) {
        report_semantic_error(step.source_line, "Cannot resolve intermediate call");
        return;
    }
    function_instance_t *instance = pending->source->active_instance;
    if (instance == NULL && global_errors == pending->errors) {
        instance = sp_instantiate(context->state, pending->source, context->environment,
                                  &pending->arguments, pending->receiver,
                                  unknown_control || context->external_unknown, step.source_line);
    }
    if (instance != NULL) {
        context->output->steps[pending->frame_step].value.symbol = instance->symbol;
        step.value.symbol = instance->symbol;
        if (instance->source->active_instance != instance) {
            operand.value = instance->returned;
            sp_apply_effects(instance->source, &instance->after, instance->receiver, context->environment,
                             pending->receiver);
        } else {
            sp_forget_call_writes(context->environment, pending->source);
        }
    }
    emit(context, step);
    operand.start = pending->frame_step;
    operand.end = context->output->count;
    operand.removable = false;
    push(context, operand);
    if (instance == NULL) {
        sp_forget_call_writes(context->environment, pending->source);
    }
    sp_pending_pop(&context->pending);
}

static void process_operator(context_t *context, rpn_step_t step, bool unknown_control) {
    operand_t operand = {.start = context->output->count, .removable = true};
    if (step.value.operator <= RPN_NOT_EQUAL && step.value.operator != RPN_ASSIGN) {
        evaluate_binary(context, step);
    } else if (step.value.operator == RPN_ASSIGN) {
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
            if (operand.value.object_type != NULL) {
                report_semantic_error(step.source_line, "Object returns are not allowed");
            }
            sp_record_return(&context->returns, operand.value);
            context->stopped = true;
        }
        emit(context, step);
    } else if (step.value.operator == RPN_DROP) {
        pop(context, step.source_line);
        emit(context, step);
    } else {
        report_semantic_error(step.source_line, "Unstructured branch in intermediate code");
    }
}

static void process_destination(context_t *context, rpn_step_t step) {
    operand_t value = {
        .destination = {
            .symbol = step.value.symbol,
            .kind = step.kind,
            .object = step.value.symbol->usage == SYMBOL_ATTRIBUTE ? context->receiver : NULL
        }
    };
    push(context, value);
}

void sp_transform_range(context_t *context, size_t begin, size_t end, sp_range_options_t options) {
    semantic_origin_t previous_origin = set_semantic_origin((semantic_origin_t){context->input, begin});
    for (size_t pc = begin; pc < end && !context->stopped; pc++) {
        set_semantic_origin((semantic_origin_t){context->input, pc});
        if ((pc != begin || options.inspect_first) && context->controls[pc] != 0) {
            pc = transform_control(context, pc, context->controls[pc] - 1, options.unknown_control) - 1;
            if (context->stopped) {
                check_discarded(context, pc + 1, end);
            }
            continue;
        }
        rpn_step_t step = context->input->steps[pc];
        switch (step.kind) {
            case RPN_SYMBOL:
                process_symbol(context, step);
                break;
            case RPN_DESTINATION:
            case RPN_ARGUMENT_DESTINATION:
                process_destination(context, step);
                break;
            case RPN_ATTRIBUTE:
            case RPN_ATTRIBUTE_DESTINATION:
                process_attribute(context, step);
                break;
            case RPN_INDEX_CHECK:
                if (step.index->is_comptime) {
                    resolve_attribute(context, &step);
                } else {
                    emit(context, step);
                }
                break;
            case RPN_CALL_FRAME:
                begin_call(context, step);
                break;
            case RPN_CALL:
                process_call(context, step, options.unknown_control);
                break;
            case RPN_OPERATOR:
                process_operator(context, step, options.unknown_control);
                break;
            case RPN_TARGET:
                report_semantic_error(step.source_line, "Unexpected standalone jump target");
                break;
            case RPN_OBJECT_VALUE:
                report_semantic_error(step.source_line, "Unexpected materialized object in a source template");
                break;
        }
        if (context->stopped) {
            check_discarded(context, pc + 1, end);
        }
    }
    set_semantic_origin(previous_origin);
}

void sp_transform(const sp_transform_request_t *request) {
    context_t context = {
        .state = request->state,
        .input = request->input,
        .output = request->output,
        .environment = request->environment,
        .function = request->function,
        .receiver = request->receiver,
        .external_unknown = request->external_unknown
    };
    context.controls = rpn_index_controls(request->input);
    sp_range_options_t initial_options = {.unknown_control = false, .inspect_first = true};
    sp_transform_range(&context, 0, request->input->count, initial_options);
    if (context.count != 0) {
        report_semantic_error(request->function != NULL ? request->function->declaration_line : 1,
                              "Intermediate body leaves an incomplete expression");
    }
    sp_pending_free_all(&context.pending);
    if (request->returned != NULL) {
        *request->returned = context.returns.value;
    }
    free(context.controls);
    free(context.stack);
}
