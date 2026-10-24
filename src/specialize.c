#include <stdlib.h>
#include <string.h>
#include "../include/specialize.h"
#include "../include/comptime_values.h"
#include "../include/comptime_objects.h"
#include "../include/diagnostics.h"
#include "../include/classes.h"

typedef struct {
    const map_node_t *symbol;
    rpn_step_kind_t kind;
    const map_node_t *object;
    const map_node_t *index;
} destination_t;

typedef struct {
    comptime_value_t value;
    destination_t destination;
    size_t start;
    size_t end;
    bool removable;
} operand_t;

typedef struct function_template {
    map_node_t *symbol;
    const rpn_t *body;
    bool contextual;
    bool external_writes;
    bool writes_attributes;
    symbol_list_t runtime_writes;
    bool used;
    struct function_template *next;
} function_template_t;

typedef struct function_instance {
    function_template_t *source;
    map_node_t *symbol;
    comptime_environment_t before;
    comptime_environment_t after;
    rpn_t body;
    comptime_value_t returned;
    bool active;
    bool unknown_entry;
    const map_node_t *receiver;
    struct function_instance *next;
} function_instance_t;

typedef struct pending_call {
    function_template_t *source;
    const map_node_t *receiver;
    comptime_environment_t arguments;
    size_t frame_step;
    int errors;
    struct pending_call *parent;
} pending_call_t;

typedef struct {
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

static void *resize(void *pointer, size_t size) {
    void *result = realloc(pointer, size);
    if (result == NULL && size != 0) {
        fprintf(stderr, "Fatal error: Cannot allocate compile-time evaluation state\n");
        exit(1);
    }
    return result;
}

static function_template_t *templates;
static function_template_t *last_template;
static function_instance_t *instances;
static function_instance_t *last_instance;
static rpn_t main_body;
static unsigned int serial;
static unsigned int depth;
static size_t evaluation_budget;

static void forget_call_writes(comptime_environment_t *environment, const function_template_t *source) {
    for (size_t i = 0; i < environment->count; i++) {
        comptime_binding_t *binding = &environment->items[i];
        if (binding->symbol->is_comptime) continue;
        bool written = source->writes_attributes &&
            (binding->object != NULL || binding->symbol->type_symbol != NULL);
        for (size_t j = 0; j < source->runtime_writes.count; j++)
            if (source->runtime_writes.items[j] == binding->symbol ||
                source->runtime_writes.items[j] == comptime_object_root(binding->object)) written = true;
        if (written) binding->value = (comptime_value_t){0};
    }
}

static void push(context_t *context, operand_t operand) {
    context->stack = resize(context->stack, (context->count + 1) * sizeof(*context->stack));
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
    if (operand.removable && operand.end == context->output->count)
        context->output->count = operand.start;
    else emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_DROP, .source_line = line});
}

static function_template_t *find_template(const map_node_t *symbol) {
    if (symbol != NULL && symbol->source_function != NULL) symbol = symbol->source_function;
    for (function_template_t *source = templates; source != NULL; source = source->next)
        if (source->symbol == symbol) return source;
    return NULL;
}

static bool is_operator(const rpn_step_t *step, rpn_operator_t operator) {
    return step->kind == RPN_OPERATOR && step->value.operator == operator;
}

static void materialize_object(context_t *context, operand_t *operand, int line) {
    map_node_t *snapshot = comptime_capture_object(operand->value, context->environment, &serial, line);
    if (snapshot == NULL) return;
    emit(context, (rpn_step_t){.kind = RPN_OBJECT_VALUE, .value.symbol = snapshot, .source_line = line});
    operand->value.object_snapshot = snapshot;
}

static bool has_comptime_effects(const rpn_t *input, size_t begin, size_t end) {
    for (size_t i = begin; i < end; i++) {
        const rpn_step_t *step = &input->steps[i];
        if (step->kind == RPN_DESTINATION && step->value.symbol->is_comptime) return true;
        if (step->kind == RPN_DESTINATION && class_has_comptime(step->value.symbol->type_symbol)) return true;
        if (step->kind == RPN_ATTRIBUTE_DESTINATION && step->value.symbol != NULL &&
            step->value.symbol->is_comptime) return true;
        if (step->kind == RPN_ATTRIBUTE_DESTINATION && step->receiver != NULL &&
            class_has_comptime(step->receiver->type_symbol)) return true;
        if (step->kind == RPN_CALL) {
            function_template_t *source = find_template(step->value.symbol);
            if (source != NULL && source->external_writes) return true;
        }
    }
    return false;
}

static void transform_range(context_t *context, size_t begin, size_t end, bool unknown_control,
                            bool inspect_first_control);
static function_instance_t *instantiate(function_template_t *source, comptime_environment_t *caller,
                                         const comptime_environment_t *arguments, const map_node_t *receiver,
                                         bool unknown_entry, int line);

static void apply_effects(const function_instance_t *instance, comptime_environment_t *caller,
                          const map_node_t *receiver) {
    forget_call_writes(caller, instance->source);
    for (size_t i = 0; i < instance->after.count; i++) {
        comptime_binding_t binding = instance->after.items[i];
        if (binding.object == instance->receiver && instance->receiver != NULL) binding.object = receiver;
        if (binding.symbol->is_comptime && instance->source->external_writes &&
            binding.symbol->owner != instance->source->symbol &&
            (binding.object == NULL || binding.object->owner != instance->source->symbol))
            comptime_set(caller, binding.symbol, binding.object, binding.value);
        if (!binding.symbol->is_comptime && binding.value.known && binding.object != NULL &&
            binding.object->owner != instance->source->symbol)
            comptime_set(caller, binding.symbol, binding.object, binding.value);
    }
    for (size_t i = 0; i < instance->source->runtime_writes.count; i++) {
        const map_node_t *symbol = instance->source->runtime_writes.items[i];
        if (symbol->usage != SYMBOL_ATTRIBUTE)
            comptime_set(caller, symbol, NULL, comptime_get(&instance->after, symbol, NULL));
    }
}

static void record_return(context_t *context, comptime_value_t value) {
    if (!context->has_return) context->returned = value;
    else if (!comptime_values_equal(context->returned, value)) context->returned = (comptime_value_t){0};
    context->has_return = true;
}

typedef struct {
    size_t start;
    size_t branch;
    size_t false_start;
    size_t body_end;
    size_t end;
    bool loop;
    bool has_else;
} control_t;

static control_t describe_control(const rpn_t *input, size_t start, size_t branch) {
    size_t false_start = input->steps[branch].value.target - 1;
    control_t control = {.start = start, .branch = branch, .false_start = false_start,
        .body_end = false_start, .end = false_start};
    if (false_start >= 2 && is_operator(&input->steps[false_start - 1], RPN_BRANCH) &&
        input->steps[false_start - 2].kind == RPN_TARGET) {
        size_t destination = input->steps[false_start - 2].value.target - 1;
        if (destination == start) {
            control.loop = true;
            control.body_end = false_start - 2;
        } else if (destination > false_start) {
            control.has_else = true;
            control.body_end = false_start - 2;
            control.end = destination;
        }
    }
    return control;
}

static operand_t process_condition(context_t *context, control_t control, bool unknown_control) {
    transform_range(context, control.start, control.branch, unknown_control, false);
    return pop(context, context->input->steps[control.branch].source_line);
}

static void process_if(context_t *context, control_t control, bool unknown_control) {
    int line = context->input->steps[control.branch].source_line;
    operand_t condition = process_condition(context, control, unknown_control);
    if (condition.value.known) {
        discard(context, condition, line);
        if (condition.value.number != 0)
            transform_range(context, control.branch + 2, control.body_end, unknown_control, true);
        else if (control.has_else)
            transform_range(context, control.false_start, control.end, unknown_control, true);
        return;
    }
    size_t false_target = rpn_reserve_target(context->output);
    emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_BRANCH_FALSE, .source_line = line});
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
    comptime_environment_t joined = comptime_join_environments(&true_environment,
        control.has_else ? context->environment : &original);
    free(context->environment->items);
    free(original.items);
    free(true_environment.items);
    *context->environment = joined;
}

static void process_loop(context_t *context, control_t control, bool unknown_control) {
    int line = context->input->steps[control.branch].source_line;
    bool expand = has_comptime_effects(context->input, control.branch + 2, control.body_end);
    size_t condition_output = context->output->count;
    operand_t condition = process_condition(context, control, unknown_control);
    if (condition.value.known && condition.value.number == 0) {
        discard(context, condition, line);
        return;
    }
    if (expand && condition.value.known && !unknown_control) {
        for (;;) {
            discard(context, condition, line);
            if (condition.value.number == 0 || context->stopped) break;
            if (evaluation_budget-- == 0) {
                report_semantic_error(line, "Compile-time loop evaluation exceeded the resource limit");
                break;
            }
            transform_range(context, control.branch + 2, control.body_end, false, true);
            if (context->stopped) break;
            condition = process_condition(context, control, false);
            if (!condition.value.known) {
                report_semantic_error(line, "Compile-time loop control became unknown");
                transform_range(context, control.branch + 2, control.body_end, true, true);
                break;
            }
        }
        return;
    }
    size_t false_target = rpn_reserve_target(context->output);
    emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_BRANCH_FALSE, .source_line = line});
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
    control_t control = describe_control(context->input, start, branch);
    if (control.loop) process_loop(context, control, unknown_control);
    else process_if(context, control, unknown_control);
    return control.end;
}

static void evaluate_binary(context_t *context, rpn_step_t step) {
    operand_t right = pop(context, step.source_line), left = pop(context, step.source_line);
    operand_t result = {.value = comptime_binary(left.value, right.value, step.value.operator)};
    result.start = left.start;
    result.removable = left.removable && right.removable && left.end == right.start &&
        right.end == context->output->count;
    emit(context, step);
    result.end = context->output->count;
    push(context, result);
}

static void assign_comptime(context_t *context, destination_t destination,
                            comptime_value_t value, bool unknown_control, int line) {
    const map_node_t *symbol = destination.symbol;
    bool argument = destination.kind == RPN_ARGUMENT_DESTINATION;
    bool local = symbol->owner == context->function ||
        (destination.object != NULL && destination.object->owner == context->function);
    bool forbidden = !argument && (unknown_control || (context->external_unknown && !local));
    if (forbidden)
        report_semantic_error(line, "Cannot modify comptime '%s' under unknown control", symbol->key);
    else if (!value.known)
        report_semantic_error(line, "Value for comptime '%s' is not known at compile time", symbol->key);
    else if (symbol->declared_type == NULL ||
        (value.floating ? strcmp(symbol->declared_type, "SINGLEF") != 0 :
                          strcmp(symbol->declared_type, "INTEGER") != 0))
        report_semantic_error(line, "Compile-time value for '%s' requires the declared type %s",
            symbol->key, symbol->declared_type != NULL ? symbol->declared_type : "numeric");
    else {
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
    rpn_step_t step = {.kind = destination.kind, .value.symbol = symbol,
        .source_line = line, .receiver = destination.object, .index = destination.index};
    emit(context, step);
    emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_ASSIGN, .source_line = line});
    if (argument && symbol != NULL && symbol->type_symbol != NULL && context->pending != NULL) {
        comptime_copy_t copy = {.source = context->environment,
            .destination = &context->pending->arguments, .function = context->function,
            .argument = true, .external_unknown = context->external_unknown};
        comptime_copy_object_fields(&copy, symbol, symbol->type_symbol, value, line);
    }
    if (symbol != NULL) class_check_copy(symbol->type_symbol, value.object_type, line);
    if (!argument && symbol != NULL && symbol->type_symbol != NULL) {
        const map_node_t *object = destination.kind == RPN_ATTRIBUTE_DESTINATION ||
            symbol->usage == SYMBOL_ATTRIBUTE ? comptime_object_location(destination.object, symbol) : symbol;
        comptime_copy_t copy = {.source = context->environment,
            .destination = context->environment, .function = context->function,
            .unknown_control = unknown_control, .external_unknown = context->external_unknown};
        comptime_copy_object_fields(&copy, object, symbol->type_symbol, value, line);
        value.object = object;
        value.object_type = symbol->type_symbol;
        value.object_snapshot = NULL;
    }
    if (!argument && destination.kind == RPN_ATTRIBUTE_DESTINATION && symbol == NULL)
        for (size_t i = 0; i < context->environment->count; i++)
            if (context->environment->items[i].object == destination.object)
                context->environment->items[i].value = (comptime_value_t){0};
    if (symbol != NULL && !argument)
        comptime_set(context->environment, symbol, destination.object, value);
}

static void process_symbol(context_t *context, rpn_step_t step) {
    operand_t operand = {.start = context->output->count, .removable = true};
    const map_node_t *symbol = step.value.symbol;
    const map_node_t *object = symbol->usage == SYMBOL_ATTRIBUTE ? context->receiver : NULL;
    operand.value = symbol->usage == SYMBOL_LEXEME ? comptime_read_constant(symbol) :
        comptime_get(context->environment, symbol, object);
    if (symbol->is_comptime) {
        if (!operand.value.known)
            report_semantic_error(step.source_line, "Comptime '%s' has no known value", symbol->key);
        else {
            const map_node_t *constant = comptime_make_constant(operand.value, step.source_line);
            if (constant != NULL) step.value.symbol = constant;
        }
    }
    operand.value.object_type = symbol->type_symbol;
    operand.value.object = operand.value.object_type == NULL ? NULL :
        (symbol->usage == SYMBOL_ATTRIBUTE ? comptime_object_location(context->receiver, symbol) : symbol);
    operand.value.object_snapshot = NULL;
    operand.start = context->output->count;
    operand.removable = true;
    emit(context, step);
    materialize_object(context, &operand, step.source_line);
    operand.end = context->output->count;
    push(context, operand);
}

static void process_attribute(context_t *context, rpn_step_t step) {
    operand_t operand = {.start = context->output->count, .removable = true};
    const map_node_t *object = step.receiver;
    const map_node_t *index = step.index;
    comptime_value_t selected = index->usage == SYMBOL_LEXEME ? comptime_read_constant(index) :
        comptime_get(context->environment, index, NULL);
    if (index->is_comptime) {
        if (!selected.known)
            report_semantic_error(step.source_line, "Comptime index '%s' has no known value", index->key);
        else step.index = comptime_make_constant(selected, step.source_line);
    }
    map_node_t *attribute = NULL;
    if (selected.known && !selected.floating) {
        if (selected.number < 0)
            report_semantic_error(step.source_line, "Attribute position %ld does not exist in class '%s'",
                (long)selected.number, object->type_symbol->key);
        else attribute = class_attribute_at(object->type_symbol, (size_t)selected.number, step.source_line);
    }
    if (attribute == NULL && !selected.known && class_has_comptime(object->type_symbol))
        report_semantic_error(step.source_line,
            "Attribute position must be known when accessing comptime attributes");
    if (step.kind == RPN_ATTRIBUTE_DESTINATION) {
        operand.destination.symbol = attribute;
        operand.destination.kind = step.kind;
        operand.destination.index = step.index;
        operand.destination.object = object;
        push(context, operand);
    } else {
        if (attribute != NULL) operand.value = comptime_get(context->environment, attribute, object);
        if (attribute != NULL && attribute->is_comptime) {
            if (!operand.value.known)
                report_semantic_error(step.source_line, "Comptime attribute '%s' has no known value", attribute->key);
            else {
                step.kind = RPN_SYMBOL;
                step.value.symbol = comptime_make_constant(operand.value, step.source_line);
            }
        }
        operand.value.object_type = attribute != NULL ? attribute->type_symbol : NULL;
        operand.value.object = operand.value.object_type != NULL ? comptime_object_location(object, attribute) : NULL;
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
    pending_call_t *pending = resize(NULL, sizeof(*pending));
    *pending = (pending_call_t){.source = find_template(step.value.symbol),
        .receiver = step.receiver != NULL ? step.receiver : context->receiver,
        .frame_step = context->output->count, .errors = global_errors, .parent = context->pending};
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
    function_instance_t *instance = NULL;
    if (global_errors == pending->errors)
        instance = instantiate(pending->source, context->environment,
            &pending->arguments, pending->receiver,
            unknown_control || context->external_unknown, step.source_line);
    if (instance != NULL) {
        context->output->steps[pending->frame_step].value.symbol = instance->symbol;
        step.value.symbol = instance->symbol;
        operand.value = instance->returned;
        apply_effects(instance, context->environment, pending->receiver);
    }
    emit(context, step);
    operand.start = pending->frame_step;
    operand.end = context->output->count;
    operand.removable = false;
    push(context, operand);
    if (instance == NULL) forget_call_writes(context->environment, pending->source);
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
                value.destination.object = step.value.symbol->usage == SYMBOL_ATTRIBUTE ? context->receiver : NULL;
                push(context, value);
                break;
            case RPN_ATTRIBUTE:
            case RPN_ATTRIBUTE_DESTINATION:
                process_attribute(context, step);
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
    }
    set_semantic_origin(previous_origin);
}

static void transform(const rpn_t *input, rpn_t *output, comptime_environment_t *environment,
                       map_node_t *function, const map_node_t *receiver, bool external_unknown,
                       comptime_value_t *returned) {
    context_t context = {.input = input, .output = output, .environment = environment,
        .function = function, .receiver = receiver, .external_unknown = external_unknown};
    context.controls = resize(NULL, (input->count + 1) * sizeof(*context.controls));
    memset(context.controls, 0, (input->count + 1) * sizeof(*context.controls));
    for (size_t i = 0; i < input->count; i++)
        if (input->steps[i].control_start != 0)
            context.controls[input->steps[i].control_start - 1] = i + 1;
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

static bool matches_environment(const function_instance_t *instance, const comptime_environment_t *environment,
                                const map_node_t *receiver, bool unknown_entry) {
    if (instance->receiver != receiver) return false;
    if (instance->unknown_entry != unknown_entry && instance->source->external_writes) return false;
    for (size_t i = 0; i < instance->before.count; i++) {
        comptime_binding_t binding = instance->before.items[i];
        if (binding.symbol->is_comptime && !comptime_values_equal(binding.value,
            comptime_get(environment, binding.symbol, binding.object))) return false;
    }
    for (size_t i = 0; i < environment->count; i++) {
        comptime_binding_t binding = environment->items[i];
        if (binding.symbol->is_comptime && !comptime_values_equal(binding.value,
            comptime_get(&instance->before, binding.symbol, binding.object))) return false;
    }
    return true;
}

static comptime_environment_t prepare_call_environment(const function_template_t *source,
                                                        const comptime_environment_t *caller,
                                                        const comptime_environment_t *arguments,
                                                        int line) {
    comptime_environment_t environment = comptime_copy_environment(caller);
    comptime_forget_runtime(&environment);
    for (size_t i = 0; i < environment.count; i++)
        if (environment.items[i].object != NULL && environment.items[i].object->owner == source->symbol)
            environment.items[i].value = (comptime_value_t){0};
    for (int i = 0; i < symbol_table->capacity; i++)
        for (map_node_t *symbol = symbol_table->buckets[i]; symbol != NULL; symbol = symbol->next)
            if (symbol->owner == source->symbol)
                comptime_set(&environment, symbol, NULL, (comptime_value_t){0});
    for (size_t i = 0; i < source->symbol->parameters.count; i++) {
        map_node_t *parameter = source->symbol->parameters.items[i];
        if (!parameter->is_comptime) continue;
        comptime_value_t argument = comptime_get(arguments, parameter, NULL);
        if (!argument.known)
            report_semantic_error(line, "Argument for comptime parameter '%s' is not known at compile time", parameter->key);
        comptime_set(&environment, parameter, NULL, argument);
    }
    for (size_t i = 0; i < arguments->count; i++) {
        comptime_binding_t binding = arguments->items[i];
        if (binding.object != NULL && binding.object->owner == source->symbol)
            comptime_set(&environment, binding.symbol, binding.object, binding.value);
    }
    return environment;
}

static function_instance_t *find_instance(function_template_t *source,
                                         const comptime_environment_t *environment,
                                         const map_node_t *receiver, bool unknown_entry) {
    for (function_instance_t *instance = instances; instance != NULL; instance = instance->next)
        if (instance->source == source && (!source->contextual ||
            matches_environment(instance, environment, receiver, unknown_entry))) return instance;
    return NULL;
}

static function_instance_t *create_instance(function_template_t *source,
                                           const comptime_environment_t *environment,
                                           const map_node_t *receiver, bool unknown_entry) {
    function_instance_t *instance = resize(NULL, sizeof(*instance));
    *instance = (function_instance_t){.source = source, .symbol = source->symbol,
        .before = comptime_copy_environment(environment), .active = true, .unknown_entry = unknown_entry,
        .receiver = receiver};
    rpn_init(&instance->body);
    if (source->contextual) {
        size_t length = strlen(source->symbol->key) + 32;
        char *name = resize(NULL, length);
        snprintf(name, length, "%s$ct%u", source->symbol->key, ++serial);
        instance->symbol = map_put(symbol_table, name, "ID");
        free(name);
        instance->symbol->usage = source->symbol->usage;
        instance->symbol->source_function = source->symbol;
        instance->symbol->declared_type = strdup(source->symbol->declared_type);
        if (instance->symbol->declared_type == NULL) {
            fprintf(stderr, "Fatal error: Cannot allocate specialization type\n");
            exit(1);
        }
        instance->symbol->declaration_line = source->symbol->declaration_line;
        for (size_t i = 0; i < source->symbol->parameters.count; i++)
            symbol_list_append(&instance->symbol->parameters, source->symbol->parameters.items[i]);
    }
    if (last_instance == NULL) instances = instance;
    else last_instance->next = instance;
    last_instance = instance;
    return instance;
}

static void record_instance_bindings(function_instance_t *instance) {
    function_template_t *source = instance->source;
    const comptime_environment_t *environment = &instance->after;
    for (size_t i = 0; i < environment->count; i++) {
        comptime_binding_t binding = environment->items[i];
        if (!binding.symbol->is_comptime || !binding.value.known ||
            binding.symbol->owner != source->symbol) continue;
        const map_node_t *constant = comptime_make_constant(binding.value, binding.symbol->declaration_line);
        if (constant == NULL) continue;
        symbol_list_append(&instance->symbol->comptime_bindings, (map_node_t *)binding.symbol);
        symbol_list_append(&instance->symbol->comptime_values, (map_node_t *)constant);
        if (!source->contextual) ((map_node_t *)binding.symbol)->comptime_value = (map_node_t *)constant;
    }
}

static function_instance_t *instantiate(function_template_t *source, comptime_environment_t *caller,
                                         const comptime_environment_t *arguments, const map_node_t *receiver,
                                         bool unknown_entry, int line) {
    comptime_environment_t environment = prepare_call_environment(source, caller, arguments, line);
    source->used = true;
    function_instance_t *instance = find_instance(source, &environment, receiver, unknown_entry);
    if (instance != NULL) {
        free(environment.items);
        return instance;
    }
    if (++depth > 128) {
        report_semantic_error(line, "Compile-time specialization exceeded the recursion resource limit");
        depth--;
        free(environment.items);
        return NULL;
    }
    instance = create_instance(source, &environment, receiver, unknown_entry);
    transform(source->body, &instance->body, &environment, source->symbol, receiver,
        unknown_entry, &instance->returned);
    instance->active = false;
    instance->after = environment;
    record_instance_bindings(instance);
    depth--;
    return instance;
}

void specialize_init(void) {
    templates = NULL;
    last_template = NULL;
    instances = NULL;
    last_instance = NULL;
    serial = 0;
    depth = 0;
    evaluation_budget = 100000;
    comptime_objects_init();
    clear_semantic_origins();
    rpn_init(&main_body);
}

void specialize_register(map_node_t *symbol, const rpn_t *body) {
    function_template_t *source = resize(NULL, sizeof(*source));
    *source = (function_template_t){.symbol = symbol, .body = body};
    if (last_template == NULL) templates = source;
    else last_template->next = source;
    last_template = source;
}

static bool record_runtime_write(function_template_t *source, map_node_t *symbol) {
    if (symbol == NULL || symbol->is_comptime || symbol->owner == source->symbol) return false;
    for (size_t i = 0; i < source->runtime_writes.count; i++)
        if (source->runtime_writes.items[i] == symbol) return false;
    symbol_list_append(&source->runtime_writes, symbol);
    return true;
}

static void analyze_function_effects(function_template_t *source) {
    for (size_t i = 0; i < source->symbol->parameters.count; i++)
        if (source->symbol->parameters.items[i]->is_comptime) source->contextual = true;
    for (size_t i = 0; i < source->body->count; i++) {
        const rpn_step_t *step = &source->body->steps[i];
        if ((step->kind == RPN_SYMBOL || step->kind == RPN_ATTRIBUTE) &&
            (class_has_comptime(step->value.symbol != NULL ? step->value.symbol->type_symbol : NULL) ||
             (step->kind == RPN_ATTRIBUTE && class_has_comptime(step->receiver->type_symbol))))
            source->contextual = true;
        if (step->kind == RPN_DESTINATION)
            record_runtime_write(source, (map_node_t *)step->value.symbol);
        if (step->kind == RPN_DESTINATION && class_has_comptime(step->value.symbol->type_symbol)) {
            source->contextual = true;
            if (step->value.symbol->owner != source->symbol) source->external_writes = true;
            source->writes_attributes = true;
        }
        if (step->kind == RPN_ATTRIBUTE_DESTINATION ||
            (step->kind == RPN_DESTINATION && step->value.symbol->usage == SYMBOL_ATTRIBUTE))
            source->writes_attributes = true;
        if (step->kind == RPN_ATTRIBUTE || step->kind == RPN_ATTRIBUTE_DESTINATION) {
            if ((step->index != NULL && step->index->is_comptime) ||
                (step->value.symbol != NULL && step->value.symbol->is_comptime))
                source->contextual = true;
            if (step->kind == RPN_ATTRIBUTE_DESTINATION &&
                class_has_comptime(step->receiver->type_symbol)) {
                source->contextual = true;
                if (step->receiver->owner != source->symbol) source->external_writes = true;
            }
        }
        if ((step->kind == RPN_SYMBOL || step->kind == RPN_DESTINATION) &&
            step->value.symbol->is_comptime && step->value.symbol->owner != source->symbol) {
            source->contextual = true;
            if (step->kind == RPN_DESTINATION) source->external_writes = true;
        }
    }
}

static void propagate_function_effects(void) {
    bool changed;
    do {
        changed = false;
        for (function_template_t *source = templates; source != NULL; source = source->next)
            for (size_t i = 0; i < source->body->count; i++) {
                const rpn_step_t *step = &source->body->steps[i];
                if (step->kind != RPN_CALL) continue;
                function_template_t *called = find_template(step->value.symbol);
                if (called != NULL && called->contextual && !source->contextual) {
                    source->contextual = true;
                    changed = true;
                }
                if (called != NULL && called->external_writes && !source->external_writes) {
                    source->external_writes = true;
                    changed = true;
                }
                if (called != NULL && called->writes_attributes && !source->writes_attributes) {
                    source->writes_attributes = true;
                    changed = true;
                }
                if (called != NULL)
                    for (size_t j = 0; j < called->runtime_writes.count; j++)
                        if (record_runtime_write(source, called->runtime_writes.items[j])) changed = true;
            }
    } while (changed);
}

bool specialize_program(const rpn_t *program) {
    for (function_template_t *source = templates; source != NULL; source = source->next)
        analyze_function_effects(source);
    propagate_function_effects();
    for (function_template_t *source = templates; source != NULL; source = source->next)
        source->symbol->comptime_template = source->contextual;
    comptime_environment_t environment = {0};
    transform(program, &main_body, &environment, NULL, NULL, false, NULL);
    for (function_template_t *source = templates; source != NULL; source = source->next) {
        if (!source->used && !source->contextual) {
            comptime_environment_t empty = {0};
            instantiate(source, &empty, &empty, NULL, false, source->symbol->declaration_line);
        }
    }
    for (size_t i = 0; i < environment.count; i++) {
        comptime_binding_t binding = environment.items[i];
        if (binding.symbol->is_comptime && binding.symbol->owner == NULL && binding.value.known)
            ((map_node_t *)binding.symbol)->comptime_value = (map_node_t *)comptime_make_constant(binding.value,
                binding.symbol->declaration_line);
    }
    free(environment.items);
    return global_errors == 0;
}

bool specialize_print(FILE *stream) {
    if (!rpn_is_complete(&main_body) || !rpn_print(stream, &main_body)) return false;
    for (function_instance_t *instance = instances; instance != NULL; instance = instance->next) {
        if (instance->active || !rpn_is_complete(&instance->body)) return false;
        if (fprintf(stream, "\n--- FUNCTION %s ---\n", instance->symbol->key) < 0 ||
            !rpn_print(stream, &instance->body)) return false;
    }
    /* Source templates remain symbolic, even when concrete instances exist. */
    for (function_template_t *source = templates; source != NULL; source = source->next) {
        if (!source->contextual) continue;
        if (fprintf(stream, "\n--- TEMPLATE %s ---\n", source->symbol->key) < 0 ||
            !rpn_print(stream, source->body)) return false;
    }
    return true;
}

void specialize_destroy(void) {
    clear_semantic_origins();
    comptime_objects_destroy();
    rpn_free(&main_body);
    while (instances != NULL) {
        function_instance_t *next = instances->next;
        rpn_free(&instances->body);
        free(instances->before.items);
        free(instances->after.items);
        free(instances);
        instances = next;
    }
    while (templates != NULL) {
        function_template_t *next = templates->next;
        free(templates->runtime_writes.items);
        free(templates);
        templates = next;
    }
}
