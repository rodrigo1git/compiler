#include <stdlib.h>
#include <string.h>
#include "../include/specialize.h"
#include "../include/comptime_values.h"
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

bool specialize_has_comptime_effects(const rpn_t *input, size_t begin, size_t end) {
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

/* A private abstract execution answers whether forgetting runtime values loses
   information required by a comptime argument. It never instantiates functions,
   emits IR, constructs constants, or reports speculative errors. */
typedef struct {
    comptime_value_t value;
    destination_t destination;
    comptime_environment_t fields;
} probe_operand_t;

typedef struct probe_call {
    function_template_t *source;
    const map_node_t *receiver;
    comptime_environment_t arguments;
    struct probe_call *parent;
} probe_call_t;

typedef struct {
    const rpn_t *input;
    comptime_environment_t environment;
    const map_node_t *receiver;
    probe_operand_t *stack;
    size_t count;
    probe_call_t *pending;
    size_t *budget;
    unsigned int depth;
    bool needs_iteration;
    bool stopped;
    bool has_return;
    comptime_value_t returned;
} probe_t;

static void probe_range(probe_t *probe, size_t begin, size_t end, bool inspect_first);

static void probe_push(probe_t *probe, probe_operand_t value) {
    probe->stack = resize(probe->stack, (probe->count + 1) * sizeof(*probe->stack));
    if (value.value.object_type != NULL)
        value.fields = comptime_copy_environment(&probe->environment);
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
    free(probe->stack);
    free(probe->environment.items);
}

static void probe_copy_object(probe_t *probe, comptime_environment_t *destination,
    const map_node_t *object, const map_node_t *type, comptime_value_t value,
    const comptime_environment_t *fields, bool argument, unsigned int depth) {
    if (type == NULL || depth == 128 || !*probe->budget) return;
    (*probe->budget)--;
    for (size_t i = 0; i < type->effective_attributes.count; i++) {
        const map_node_t *field = type->effective_attributes.items[i];
        comptime_value_t selected = {0};
        if (value.object_type && i < value.object_type->effective_attributes.count)
            selected = comptime_object_field(value, value.object_type->effective_attributes.items[i], fields);
        if (field->type_symbol) {
            const map_node_t *child = comptime_object_location(object, field);
            probe_copy_object(probe, destination, child, field->type_symbol, selected,
                fields, argument, depth + 1);
            selected.object = child;
            selected.object_type = field->type_symbol;
            selected.object_snapshot = NULL;
        } else if (field->is_comptime && !selected.known) probe->needs_iteration = true;
        if (!argument || field->is_comptime)
            comptime_set(destination, field, object, selected);
    }
}

static comptime_value_t probe_invoke(probe_t *probe, probe_call_t *call) {
    function_template_t *source = call->source;
    if (source == NULL) return (comptime_value_t){0};
    for (size_t i = 0; i < source->symbol->parameters.count; i++) {
        const map_node_t *parameter = source->symbol->parameters.items[i];
        if (parameter->is_comptime && !comptime_get(&call->arguments, parameter, NULL).known)
            probe->needs_iteration = true;
    }
    if (probe->needs_iteration || probe->depth == 128 || !*probe->budget) {
        forget_call_writes(&probe->environment, source);
        return (comptime_value_t){0};
    }
    probe_t callee = {.input = source->body, .receiver = call->receiver,
        .environment = comptime_copy_environment(&probe->environment),
        .depth = probe->depth + 1, .budget = probe->budget};
    comptime_forget_runtime(&callee.environment);
    for (size_t i = 0; i < callee.environment.count; i++) {
        comptime_binding_t *binding = &callee.environment.items[i];
        if (binding->symbol->owner == source->symbol ||
            (binding->object && binding->object->owner == source->symbol))
            binding->value = (comptime_value_t){0};
    }
    for (size_t i = 0; i < call->arguments.count; i++) {
        comptime_binding_t binding = call->arguments.items[i];
        if (binding.symbol->is_comptime)
            comptime_set(&callee.environment, binding.symbol, binding.object, binding.value);
    }
    probe_range(&callee, 0, source->body->count, true);
    probe->needs_iteration |= callee.needs_iteration;
    forget_call_writes(&probe->environment, source);
    /* Apply only the effects that normal specialization exports to its caller. */
    for (size_t i = 0; i < callee.environment.count; i++) {
        comptime_binding_t binding = callee.environment.items[i];
        if ((binding.symbol->is_comptime && source->external_writes &&
             binding.symbol->owner != source->symbol &&
             (!binding.object || binding.object->owner != source->symbol)) ||
            (!binding.symbol->is_comptime && binding.value.known && binding.object &&
             binding.object->owner != source->symbol))
            comptime_set(&probe->environment, binding.symbol, binding.object, binding.value);
    }
    for (size_t i = 0; i < source->runtime_writes.count; i++) {
        const map_node_t *symbol = source->runtime_writes.items[i];
        if (symbol->usage != SYMBOL_ATTRIBUTE)
            comptime_set(&probe->environment, symbol, NULL,
                comptime_get(&callee.environment, symbol, NULL));
    }
    comptime_value_t returned = callee.returned;
    probe_destroy(&callee);
    return returned;
}

static void probe_step(probe_t *probe, const rpn_step_t *step) {
    probe_operand_t operand = {0};
    switch (step->kind) {
    case RPN_SYMBOL:
        operand.value = comptime_read_symbol(&probe->environment, step->value.symbol, probe->receiver);
        operand.value.object_type = step->value.symbol->type_symbol;
        if (operand.value.object_type)
            operand.value.object = comptime_resolve_object(step->value.symbol, probe->receiver);
        probe_push(probe, operand);
        break;
    case RPN_DESTINATION: case RPN_ARGUMENT_DESTINATION:
        operand.destination = (destination_t){.symbol = step->value.symbol, .kind = step->kind,
            .object = step->value.symbol->usage == SYMBOL_ATTRIBUTE ? probe->receiver : NULL};
        probe_push(probe, operand);
        break;
    case RPN_ATTRIBUTE: case RPN_ATTRIBUTE_DESTINATION: {
        const map_node_t *object = comptime_resolve_object(step->receiver, probe->receiver);
        comptime_value_t index = comptime_read_symbol(&probe->environment, step->index, probe->receiver);
        const map_node_t *field = NULL;
        if (index.known && !index.floating && index.number >= 0 && object &&
            index.number < object->type_symbol->effective_attributes.count)
            field = object->type_symbol->effective_attributes.items[(size_t)index.number];
        if (step->index->is_comptime && !index.known) probe->needs_iteration = true;
        if (step->kind == RPN_ATTRIBUTE_DESTINATION)
            operand.destination = (destination_t){.symbol = field, .object = object, .kind = step->kind};
        else if (field)
            operand.value = comptime_object_field((comptime_value_t){.object = object,
                .object_type = object->type_symbol}, field, &probe->environment);
        probe_push(probe, operand);
        break;
    }
    case RPN_CALL_FRAME: {
        probe_call_t *call = resize(NULL, sizeof(*call));
        *call = (probe_call_t){.source = find_template(step->value.symbol),
            .receiver = comptime_resolve_object(step->receiver, probe->receiver), .parent = probe->pending};
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
    case RPN_OPERATOR: {
        rpn_operator_t op = step->value.operator;
        probe_operand_t right = probe_pop(probe);
        if (op <= RPN_NOT_EQUAL) {
            probe_operand_t left = probe_pop(probe);
            if (op == RPN_ASSIGN) {
                destination_t destination = right.destination;
                bool argument = destination.kind == RPN_ARGUMENT_DESTINATION && probe->pending;
                comptime_environment_t *environment = argument ?
                    &probe->pending->arguments : &probe->environment;
                if (destination.symbol) {
                    if (destination.symbol->type_symbol) {
                        const map_node_t *object = destination.object ?
                            comptime_object_location(destination.object, destination.symbol) : destination.symbol;
                        /* Keep the source snapshot separate, including self-copies. */
                        probe_copy_object(probe, environment, object, destination.symbol->type_symbol,
                            left.value, &left.fields, argument, 0);
                    }
                    comptime_set(environment, destination.symbol, destination.object, left.value);
                } else if (!argument) {
                    for (size_t i = 0; i < environment->count; i++)
                        if (environment->items[i].object == destination.object)
                            environment->items[i].value = (comptime_value_t){0};
                }
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
            else if (!comptime_values_equal(probe->returned, right.value)) probe->returned = (comptime_value_t){0};
            probe->has_return = true;
            probe->stopped = true;
        }
        free(right.fields.items);
        break;
    }
    case RPN_INDEX_CHECK: case RPN_TARGET: case RPN_OBJECT_VALUE: break;
    }
}

static comptime_value_t probe_condition(probe_t *probe, control_t control) {
    probe_range(probe, control.start, control.branch, false);
    probe_operand_t condition = probe_pop(probe);
    free(condition.fields.items);
    return condition.value;
}

static void probe_control(probe_t *probe, control_t control) {
    comptime_value_t condition = probe_condition(probe, control);
    if (condition.known && condition.number == 0 && control.loop) return;
    if (control.loop) {
        probe_t attempt = {.input = probe->input, .receiver = probe->receiver,
            .environment = comptime_copy_environment(&probe->environment),
            .depth = probe->depth, .budget = probe->budget};
        comptime_forget_runtime(&attempt.environment);
        probe_range(&attempt, control.branch + 2, control.body_end, true);
        bool needs_iteration = attempt.needs_iteration;
        bool expand = needs_iteration ||
            specialize_has_comptime_effects(probe->input, control.branch + 2, control.body_end);
        probe_destroy(&attempt);
        if (!expand || !condition.known) {
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
    if (condition.known) {
        if (condition.number != 0) probe_range(probe, control.branch + 2, control.body_end, true);
        else if (control.has_else) probe_range(probe, control.false_start, control.end, true);
        return;
    }
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
}

static void probe_range(probe_t *probe, size_t begin, size_t end, bool inspect_first) {
    for (size_t pc = begin; pc < end && !probe->stopped && !probe->needs_iteration; pc++) {
        if (!*probe->budget) return;
        (*probe->budget)--;
        size_t branch = pc;
        if (pc != begin || inspect_first)
            while (branch < end && probe->input->steps[branch].control_start != pc + 1) branch++;
        else branch = end;
        if (branch < end) {
            control_t control = describe_control(probe->input, pc, branch);
            probe_control(probe, control);
            pc = control.end - 1;
        } else probe_step(probe, &probe->input->steps[pc]);
    }
}

bool specialize_requires_iteration(const rpn_t *input, size_t begin, size_t end,
    const comptime_environment_t *environment, const map_node_t *receiver) {
    size_t budget = 100000;
    probe_t probe = {.input = input, .environment = comptime_copy_environment(environment),
        .receiver = receiver, .budget = &budget};
    comptime_forget_runtime(&probe.environment);
    probe_range(&probe, begin, end, true);
    bool result = probe.needs_iteration;
    probe_destroy(&probe);
    return result;
}

static operand_t process_condition(context_t *context, control_t control, bool unknown_control) {
    transform_range(context, control.start, control.branch, unknown_control, false);
    return pop(context, context->input->steps[control.branch].source_line);
}

static void check_discarded(context_t *context, size_t begin, size_t end) {
    if (begin < end)
        types_check_discarded(context->input, context->function, begin, end,
                              context->environment, context->receiver);
}

static void process_if(context_t *context, control_t control, bool unknown_control) {
    int line = context->input->steps[control.branch].source_line;
    size_t condition_output = context->output->count;
    operand_t condition = process_condition(context, control, unknown_control);
    if (condition.value.known) {
        discard(context, condition, line);
        if (condition.value.number != 0 && control.has_else)
            check_discarded(context, control.false_start, control.end);
        else if (condition.value.number == 0)
            check_discarded(context, control.branch + 2, control.body_end);
        if (condition.value.number != 0)
            transform_range(context, control.branch + 2, control.body_end, unknown_control, true);
        else if (control.has_else)
            transform_range(context, control.false_start, control.end, unknown_control, true);
        return;
    }
    size_t false_target = rpn_reserve_target(context->output);
    context->output->steps[false_target - 1].control_start = condition_output + 1;
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
    bool effects = specialize_has_comptime_effects(context->input, control.branch + 2, control.body_end);
    bool expand = effects;
    size_t condition_output = context->output->count;
    operand_t condition = process_condition(context, control, unknown_control);
    if (condition.value.known && condition.value.number == 0) {
        check_discarded(context, control.branch + 2, control.body_end);
        discard(context, condition, line);
        return;
    }
    if (!expand && condition.value.known)
        expand = specialize_requires_iteration(context->input, control.branch + 2, control.body_end,
            context->environment, context->receiver);
    if (expand && condition.value.known && (!unknown_control || !effects)) {
        for (;;) {
            discard(context, condition, line);
            if (condition.value.number == 0 || context->stopped) break;
            if (evaluation_budget-- == 0) {
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
    context->output->steps[false_target - 1].loop_variable = context->input->steps[control.branch].loop_variable;
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
    else if (value.error)
        report_semantic_error(line, "%s required by comptime assignment", numeric_error_message(value.error));
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
        .source_line = line, .receiver = destination.receiver, .index = destination.index};
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
    operand.value.object = operand.value.object_type == NULL ? NULL :
        comptime_resolve_object(symbol, context->receiver);
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
    const map_node_t *object = comptime_resolve_object(step.receiver, context->receiver);
    const map_node_t *index = step.index;
    comptime_value_t selected = comptime_read_symbol(context->environment, index, context->receiver);
    if (index->is_comptime) {
        if (!selected.known)
            report_semantic_error(step.source_line, "Comptime index '%s' has no known value", index->key);
        else step.index = comptime_make_constant(selected, step.source_line);
    }
    bool runtime_index = !index->is_comptime && index->usage != SYMBOL_LEXEME;
    map_node_t *attribute = NULL;
    if (runtime_index && selected.known && (selected.number < 0 ||
        selected.number >= object->type_symbol->effective_attributes.count)) selected.known = false;
    if (selected.known && !selected.floating) {
        if (selected.number < 0)
            report_semantic_error(step.source_line, "Attribute position %ld does not exist in class '%s'",
                (long)selected.number, object->type_symbol->key);
        else attribute = class_attribute_at(object->type_symbol, (size_t)selected.number, step.source_line);
    }
    if (((attribute == NULL && !selected.known) || runtime_index) && class_has_comptime(object->type_symbol)) {
        if (step.kind == RPN_ATTRIBUTE) {
            comptime_value_t receiver = {.object_type = object->type_symbol, .object = object};
            step.capture = comptime_capture_object(receiver, context->environment, &serial, step.source_line);
        } else report_semantic_error(step.source_line,
            "Attribute position must be known when accessing comptime attributes");
    }
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
        .receiver = comptime_resolve_object(step.receiver, context->receiver),
        .frame_step = context->output->count, .errors = global_errors, .parent = context->pending};
    context->pending = pending;
    emit(context, step);
}

static function_instance_t *find_active_instance(const function_template_t *source) {
    for (function_instance_t *instance = instances; instance; instance = instance->next)
        if (instance->active && instance->source == source) return instance;
    return NULL;
}

static void process_call(context_t *context, rpn_step_t step, bool unknown_control) {
    operand_t operand = {.start = context->output->count, .removable = true};
    pending_call_t *pending = context->pending;
    if (pending == NULL || pending->source == NULL) {
        report_semantic_error(step.source_line, "Cannot resolve intermediate call");
        return;
    }
    function_instance_t *instance = find_active_instance(pending->source);
    if (!instance && global_errors == pending->errors)
        instance = instantiate(pending->source, context->environment,
            &pending->arguments, pending->receiver,
            unknown_control || context->external_unknown, step.source_line);
    if (instance != NULL) {
        context->output->steps[pending->frame_step].value.symbol = instance->symbol;
        step.value.symbol = instance->symbol;
        if (!instance->active) {
            operand.value = instance->returned;
            apply_effects(instance, context->environment, pending->receiver);
        } else forget_call_writes(context->environment, pending->source);
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
                value.destination.object = step.value.symbol->usage == SYMBOL_ATTRIBUTE ? context->receiver : NULL;
                push(context, value);
                break;
            case RPN_ATTRIBUTE:
            case RPN_ATTRIBUTE_DESTINATION:
                process_attribute(context, step);
                break;
            case RPN_INDEX_CHECK:
                emit(context, step);
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

comptime_value_t specialize_evaluate_call(const map_node_t *symbol,
    comptime_environment_t *environment, const comptime_environment_t *arguments,
    const map_node_t *receiver, int line) {
    function_template_t *source = find_template(symbol);
    if (!source || find_active_instance(source)) return (comptime_value_t){0};
    function_instance_t *instance = instantiate(source, environment, arguments, receiver, false, line);
    if (!instance) return (comptime_value_t){0};
    apply_effects(instance, environment, receiver);
    return instance->returned;
}

size_t specialize_source_count(void) {
    size_t count = 0;
    for (function_template_t *item = templates; item; item = item->next) count++;
    return count;
}
const rpn_t *specialize_source_body(size_t index, map_node_t **symbol) {
    function_template_t *item = templates;
    while (item && index--) item = item->next;
    if (!item) return NULL;
    *symbol = item->symbol;
    return item->body;
}
size_t specialize_instance_count(void) {
    size_t count = 0;
    for (function_instance_t *item = instances; item; item = item->next) count++;
    return count;
}
const rpn_t *specialize_instance_body(size_t index, map_node_t **symbol) {
    function_instance_t *item = instances;
    while (item && index--) item = item->next;
    if (!item) return NULL;
    *symbol = item->symbol;
    return &item->body;
}
const rpn_t *specialize_main_body(void) { return &main_body; }
