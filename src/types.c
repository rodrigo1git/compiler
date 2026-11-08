#include <stdlib.h>
#include <string.h>
#include "../include/types.h"
#include "../include/classes.h"
#include "../include/diagnostics.h"
#include "../include/specialize.h"
#include "../include/comptime_values.h"
#include "../include/comptime_objects.h"

type_t type_of_symbol(const map_node_t *symbol) {
    if (!symbol) return (type_t){TYPE_ERROR, NULL};
    if (symbol->type_symbol) return (type_t){TYPE_OBJECT, symbol->type_symbol};
    const char *name = symbol->declared_type ? symbol->declared_type : symbol->value;
    type_kind_t kind = TYPE_ERROR;
    if (!strcmp(name, "INTEGER")) kind = TYPE_INTEGER;
    if (!strcmp(name, "SINGLEF")) kind = TYPE_SINGLEF;
    if (!strcmp(name, "CHAIN")) kind = TYPE_STRING;
    return (type_t){kind, NULL};
}

const char *type_name(type_t type) {
    static const char *names[] = {"invalid", "INTEGER", "SINGLEF", "CHAIN", "object",
                                   "variable-position attribute", "comptime-dependent attribute"};
    return type.kind == TYPE_OBJECT ? type.class_symbol->key : names[type.kind];
}

type_t type_of_attribute(const rpn_step_t *step) {
    const map_node_t *kind = step->receiver->type_symbol;
    if (step->index->usage == SYMBOL_LEXEME) {
        if (type_of_symbol(step->index).kind != TYPE_INTEGER) return (type_t){TYPE_ERROR, NULL};
        long position = strtol(step->index->key, NULL, 10);
        if (position < 0 || (size_t)position >= kind->class_info->effective_attributes.count)
            return (type_t){TYPE_ERROR, NULL};
        return type_of_symbol(kind->class_info->effective_attributes.items[position]);
    }
    if (step->index->is_comptime) return (type_t){TYPE_COMPTIME_PENDING, kind};
    type_t result = {TYPE_CANDIDATES, kind};
    if (kind->class_info->effective_attributes.count) {
        result = type_of_symbol(kind->class_info->effective_attributes.items[0]);
        for (size_t i = 1; i < kind->class_info->effective_attributes.count; i++) {
            type_t next = type_of_symbol(kind->class_info->effective_attributes.items[i]);
            if (next.kind != result.kind || next.class_symbol != result.class_symbol)
                return (type_t){TYPE_CANDIDATES, kind};
        }
    }
    return result;
}

bool types_compatible(type_t destination, type_t source) {
    if (destination.kind == TYPE_ERROR || source.kind == TYPE_ERROR ||
        destination.kind == TYPE_COMPTIME_PENDING || source.kind == TYPE_COMPTIME_PENDING) return true;
    if (destination.kind == TYPE_CANDIDATES) {
        for (size_t i = 0; i < destination.class_symbol->class_info->effective_attributes.count; i++)
            if (!types_compatible(type_of_symbol(destination.class_symbol->class_info->effective_attributes.items[i]), source)) return false;
        return true;
    }
    if (source.kind == TYPE_CANDIDATES) {
        for (size_t i = 0; i < source.class_symbol->class_info->effective_attributes.count; i++)
            if (!types_compatible(destination, type_of_symbol(source.class_symbol->class_info->effective_attributes.items[i]))) return false;
        return true;
    }
    if (destination.kind != source.kind) return false;
    if (source.kind != TYPE_OBJECT) return true;
    const map_node_t *a = destination.class_symbol, *b = source.class_symbol;
    return a == b || (a->class_info->class_code && b->class_info->class_code && !strcmp(a->class_info->class_code, b->class_info->class_code) && class_same_layout(a, b));
}

static bool numeric(type_t type) {
    if (type.kind == TYPE_COMPTIME_PENDING || type.kind == TYPE_ERROR) return true;
    if (type.kind == TYPE_CANDIDATES) {
        for (size_t i = 0; i < type.class_symbol->class_info->effective_attributes.count; i++)
            if (!numeric(type_of_symbol(type.class_symbol->class_info->effective_attributes.items[i]))) return false;
        return true;
    }
    return type.kind == TYPE_INTEGER || type.kind == TYPE_SINGLEF;
}

static type_t binary_type(type_t left, type_t right, rpn_operator_t op, int line) {
    if (!numeric(left) || !numeric(right) || !types_compatible(left, right)) {
        report_semantic_error(line, "Incompatible operand types: %s and %s", type_name(left), type_name(right));
        return (type_t){TYPE_ERROR, NULL};
    }
    if (op >= RPN_LESS) return (type_t){TYPE_INTEGER, NULL};
    if (left.kind == TYPE_COMPTIME_PENDING || right.kind == TYPE_COMPTIME_PENDING) return (type_t){TYPE_COMPTIME_PENDING, NULL};
    return left;
}

static bool check_body(const rpn_t *body, const map_node_t *function,
                       const rpn_t *origin, size_t first_step) {
    semantic_origin_t previous = set_semantic_origin((semantic_origin_t){origin, first_step});
    int errors = global_errors;
    type_t *stack = calloc(body->count + 2, sizeof(*stack));
    if (!stack) { fprintf(stderr, "Cannot allocate type stack\n"); exit(1); }
    size_t count = 0;
    for (size_t pc = 0; pc < body->count; pc++) {
        set_semantic_origin((semantic_origin_t){origin, first_step + pc});
        const rpn_step_t *step = &body->steps[pc];
        type_t value = {TYPE_ERROR, NULL};
        int line = step->source_line ? step->source_line : function ? function->declaration_line : 1;
        switch (step->kind) {
        case RPN_SYMBOL: case RPN_DESTINATION: case RPN_ARGUMENT_DESTINATION:
            stack[count++] = type_of_symbol(step->value.symbol); break;
        case RPN_ATTRIBUTE: case RPN_ATTRIBUTE_DESTINATION:
            if (type_of_symbol(step->index).kind != TYPE_INTEGER)
                report_semantic_error(line, "Attribute index requires INTEGER; received %s", type_name(type_of_symbol(step->index)));
            if (!step->index->is_comptime && step->index->usage != SYMBOL_LEXEME) {
                const symbol_list_t *fields = &step->receiver->type_symbol->class_info->effective_attributes;
                for (size_t i = 0; i < fields->count; i++) {
                    class_attribute_at(step->receiver->type_symbol, i, line);
                    if (step->kind == RPN_ATTRIBUTE_DESTINATION && fields->items[i]->is_comptime)
                        report_semantic_error(line, "Writing a comptime attribute requires a compile-time index");
                }
            }
            stack[count++] = type_of_attribute(step); break;
        case RPN_CALL: stack[count++] = type_of_symbol(step->value.symbol); break;
        case RPN_CALL_FRAME: case RPN_INDEX_CHECK: break;
        case RPN_OBJECT_VALUE: break;
        case RPN_TARGET:
            if (step->loop_variable && type_of_symbol(step->loop_variable).kind != TYPE_INTEGER)
                report_semantic_error(line, "FROM control variable requires INTEGER");
            stack[count++] = (type_t){TYPE_INTEGER, NULL}; break;
        case RPN_OPERATOR: {
            rpn_operator_t op = step->value.operator;
            size_t needed = (op <= RPN_NOT_EQUAL || op == RPN_BRANCH_FALSE) ? 2 : 1;
            if (count < needed) {
                report_semantic_error(line, "Incomplete intermediate operand stack during type checking");
                count = 0; break;
            }
            type_t right = stack[--count];
            if (op <= RPN_NOT_EQUAL) {
                type_t left = stack[--count];
                if (op == RPN_ASSIGN) {
                    if (!types_compatible(right, left))
                        report_semantic_error(line, "Incompatible assignment types: %s := %s", type_name(right), type_name(left));
                } else stack[count++] = binary_type(left, right, op, line);
            } else if (op == RPN_TO_INTEGER) {
                if (!types_compatible((type_t){TYPE_SINGLEF, NULL}, right))
                    report_semantic_error(line, "TOI requires SINGLEF; received %s", type_name(right));
                stack[count++] = (type_t){TYPE_INTEGER, NULL};
            } else if (op == RPN_DUPLICATE) { stack[count++] = right; stack[count++] = right; }
            else if (op == RPN_RETURN) {
                if (right.kind == TYPE_OBJECT)
                    report_semantic_error(line, "Object returns are not allowed");
                else if (!function || !types_compatible(type_of_symbol(function), right))
                    report_semantic_error(line, "Incompatible return type: %s", type_name(right));
            } else if (op == RPN_OUTPUT) {
                if (!numeric(right) && right.kind != TYPE_STRING)
                    report_semantic_error(line, "POUT requires a numeric expression or string");
            } else if (op == RPN_BRANCH_FALSE) {
                value = stack[--count];
                if (!types_compatible((type_t){TYPE_INTEGER, NULL}, value))
                    report_semantic_error(line, "Condition requires an INTEGER comparison result");
            }
            break;
        }
        }
    }
    if (count) report_semantic_error(function ? function->declaration_line : 1, "Intermediate body leaves operands during type checking");
    free(stack);
    set_semantic_origin(previous);
    return errors == global_errors;
}

bool types_check_body(const rpn_t *body, const map_node_t *function) {
    return check_body(body, function, body, 0);
}

bool types_check_sources(const rpn_t *main_body) {
    int errors = global_errors;
    if (!class_check_containment()) return false;
    types_check_body(main_body, NULL);
    for (size_t i = 0; i < specialize_source_count(); i++) {
        map_node_t *symbol;
        const rpn_t *body = specialize_source_body(i, &symbol);
        types_check_body(body, symbol);
    }
    return errors == global_errors;
}

bool types_check_specializations(void) {
    int errors = global_errors;
    types_check_body(specialize_main_body(), NULL);
    for (size_t i = 0; i < specialize_instance_count(); i++) {
        map_node_t *symbol;
        const rpn_t *body = specialize_instance_body(i, &symbol);
        types_check_body(body, symbol);
    }
    return errors == global_errors;
}

typedef struct {
    comptime_value_t value;
    const map_node_t *destination;
    const map_node_t *object;
    const comptime_environment_t *fields;
    bool argument;
} type_value_t;

typedef struct {
    const map_node_t *symbol;
    const map_node_t *receiver;
    comptime_environment_t arguments;
    int errors;
} type_call_t;

typedef struct {
    rpn_t body;
    const rpn_t *origin;
    const map_node_t *function;
    const map_node_t *receiver;
    comptime_environment_t environment;
    type_value_t *stack;
    size_t count;
    type_call_t *calls;
    size_t call_count;
    comptime_environment_t *snapshots;
    size_t snapshot_count;
    size_t remaining_iterations;
    size_t *controls;
} type_resolution_t;

static void clear_type_snapshots(type_resolution_t *context) {
    for (size_t i = 0; i < context->snapshot_count; i++) free(context->snapshots[i].items);
    context->snapshot_count = 0;
}

static void push_type_value(type_resolution_t *context, type_value_t value) {
    if (value.value.object_type && !value.destination) {
        comptime_environment_t *snapshot = &context->snapshots[context->snapshot_count++];
        *snapshot = comptime_copy_environment(&context->environment);
        value.fields = snapshot;
    }
    context->stack[context->count++] = value;
}

static void resolve_step(type_resolution_t *context, size_t pc) {
    rpn_step_t *step = &context->body.steps[pc];
    *step = context->origin->steps[pc];
    type_value_t value = {0};
    switch (step->kind) {
    case RPN_SYMBOL: {
        const map_node_t *symbol = step->value.symbol;
        value.value = comptime_read_symbol(&context->environment, symbol, context->receiver);
        value.value.object_type = symbol->type_symbol;
        if (symbol->type_symbol)
            value.value.object = comptime_resolve_object(symbol, context->receiver);
        push_type_value(context, value);
        break;
    }
    case RPN_DESTINATION: case RPN_ARGUMENT_DESTINATION:
        value.destination = step->value.symbol;
        value.object = value.destination->usage == SYMBOL_ATTRIBUTE ? context->receiver : NULL;
        value.argument = step->kind == RPN_ARGUMENT_DESTINATION;
        context->stack[context->count++] = value;
        break;
    case RPN_ATTRIBUTE: case RPN_ATTRIBUTE_DESTINATION: {
        comptime_value_t index = comptime_read_symbol(&context->environment, step->index, context->receiver);
        if (step->index->is_comptime) {
            if (index.known) step->index = comptime_make_constant(index, step->source_line);
            else report_semantic_error(step->source_line,
                "Cannot determine comptime index '%s' when checking discarded source", step->index->key);
        }
        const map_node_t *field = comptime_select_field(step->receiver->type_symbol, index);
        if (field) {
            const map_node_t *receiver = comptime_resolve_object(step->receiver, context->receiver);
            comptime_value_t object = {.object = receiver, .object_type = step->receiver->type_symbol};
            value.value = comptime_object_field(object, field, &context->environment);
            value.destination = field;
            value.object = receiver;
        }
        if (step->index && step->index->usage == SYMBOL_LEXEME && index.known && !index.floating) {
            if (index.number < 0)
                report_semantic_error(step->source_line, "Attribute position %ld does not exist in class '%s'",
                    (long)index.number, step->receiver->type_symbol->key);
            else class_attribute_at(step->receiver->type_symbol, (size_t)index.number, step->source_line);
        }
        if (step->kind == RPN_ATTRIBUTE) value.destination = NULL;
        push_type_value(context, value);
        break;
    }
    case RPN_CALL_FRAME:
        context->calls[context->call_count++] = (type_call_t){.symbol = step->value.symbol,
            .receiver = comptime_resolve_object(step->receiver, context->receiver), .errors = global_errors};
        break;
    case RPN_CALL: {
        if (!context->call_count) break;
        type_call_t *call = &context->calls[--context->call_count];
        if (call->errors == global_errors)
            value.value = specialize_evaluate_call(call->symbol, &context->environment,
                &call->arguments, call->receiver, false, step->source_line);
        free(call->arguments.items);
        context->stack[context->count++] = value;
        break;
    }
    case RPN_OPERATOR: {
        rpn_operator_t op = step->value.operator;
        if (!context->count) break;
        type_value_t right = context->stack[--context->count];
        if (op <= RPN_NOT_EQUAL) {
            if (!context->count) break;
            type_value_t left = context->stack[--context->count];
            if (op == RPN_ASSIGN) {
                if (right.destination) {
                    comptime_environment_t *destination = right.argument && context->call_count ?
                        &context->calls[context->call_count - 1].arguments : &context->environment;
                    if (right.destination->type_symbol) {
                        const map_node_t *object = right.object ?
                            comptime_object_location(right.object, right.destination) : right.destination;
                        comptime_copy_t copy = {.source = left.fields ? left.fields : &context->environment,
                            .destination = destination, .argument = right.argument};
                        comptime_copy_object_fields(&copy, object, right.destination->type_symbol,
                            left.value, step->source_line);
                    }
                    comptime_set(destination, right.destination, right.object, left.value);
                }
            } else {
                value.value = comptime_binary(left.value, right.value, op);
                context->stack[context->count++] = value;
            }
        } else if (op == RPN_TO_INTEGER) {
            right.value = comptime_to_integer(right.value);
            context->stack[context->count++] = right;
        } else if (op == RPN_DUPLICATE) {
            context->stack[context->count++] = right;
            context->stack[context->count++] = right;
        }
        break;
    }
    case RPN_TARGET: case RPN_OBJECT_VALUE: case RPN_INDEX_CHECK: break;
    }
}

static void resolve_range(type_resolution_t *context, size_t begin, size_t end);

static bool check_resolved_range(type_resolution_t *context, size_t begin, size_t end) {
    rpn_t slice = {.steps = context->body.steps + begin, .count = end - begin};
    return check_body(&slice, context->function, context->origin, begin);
}

static bool same_type_environment(const comptime_environment_t *left,
                                   const comptime_environment_t *right) {
    if (left->count != right->count) return false;
    for (size_t i = 0; i < left->count; i++) {
        comptime_binding_t binding = left->items[i];
        if (!comptime_values_equal(binding.value, comptime_get(right, binding.symbol, binding.object)))
            return false;
    }
    return true;
}

static void resolve_loop(type_resolution_t *context, size_t begin, size_t target, size_t body_end) {
    int errors = global_errors;
    for (;;) {
        if (!context->remaining_iterations) {
            report_semantic_error(context->origin->steps[target].source_line,
                "Discarded loop type checking exceeded the iteration limit");
            return;
        }
        context->remaining_iterations--;
        comptime_environment_t before = comptime_copy_environment(&context->environment);
        resolve_range(context, target + 2, body_end);
        if (errors == global_errors) check_resolved_range(context, target + 2, body_end);
        clear_type_snapshots(context);
        if (errors != global_errors) { free(before.items); return; }
        resolve_range(context, begin, target);
        comptime_value_t condition = context->count ? context->stack[--context->count].value : (comptime_value_t){0};
        if (errors == global_errors) check_resolved_range(context, begin, target + 2);
        bool stable = same_type_environment(&before, &context->environment);
        if (!condition.known) {
            comptime_environment_t joined = comptime_join_environments(&before, &context->environment);
            free(context->environment.items);
            context->environment = joined;
        }
        free(before.items);
        if (errors != global_errors || !condition.known || condition.number == 0 || stable) return;
    }
}

static size_t resolve_control(type_resolution_t *context, size_t begin, size_t target) {
    rpn_control_t control = rpn_describe_control(&context->body, begin, target);
    size_t false_start = control.false_start, body_end = control.body_end, finish = control.end;
    bool has_else = control.has_else;
    resolve_range(context, begin, target);
    comptime_value_t condition = context->count ? context->stack[--context->count].value : (comptime_value_t){0};
    if (control.loop && condition.known && condition.number != 0 &&
        (specialize_has_comptime_effects(context->origin, target + 2, body_end) ||
         specialize_requires_iteration(context->origin, target + 2, body_end,
             &context->environment, context->receiver))) {
        if (check_resolved_range(context, begin, target + 2))
            resolve_loop(context, begin, target, body_end);
        return finish;
    }
    comptime_environment_t original = comptime_copy_environment(&context->environment);
    resolve_range(context, target + 2, body_end);
    comptime_environment_t true_environment = context->environment;
    context->environment = comptime_copy_environment(&original);
    if (has_else) resolve_range(context, false_start, finish);
    comptime_environment_t result;
    if (condition.known && (!control.loop || condition.number == 0))
        result = comptime_copy_environment(condition.number != 0 ? &true_environment : &context->environment);
    else result = comptime_join_environments(&true_environment, &context->environment);
    free(original.items);
    free(true_environment.items);
    free(context->environment.items);
    context->environment = result;
    return finish;
}

static void resolve_range(type_resolution_t *context, size_t begin, size_t end) {
    semantic_origin_t previous = set_semantic_origin((semantic_origin_t){context->origin, begin});
    for (size_t pc = begin; pc < end; pc++) {
        set_semantic_origin((semantic_origin_t){context->origin, pc});
        size_t target = context->controls[pc] ? context->controls[pc] - 1 : end;
        if (target < end) pc = resolve_control(context, pc, target) - 1;
        else resolve_step(context, pc);
    }
    set_semantic_origin(previous);
}

bool types_check_discarded(const rpn_t *body, const map_node_t *function,
                           size_t begin, size_t end,
                           const comptime_environment_t *environment,
                           const map_node_t *receiver) {
    int errors = global_errors;
    type_resolution_t context = {.body = *body, .origin = body, .function = function,
        .receiver = receiver, .remaining_iterations = 100000,
        .environment = comptime_copy_environment(environment)};
    context.body.steps = calloc(body->count + 1, sizeof(*body->steps));
    context.stack = calloc(body->count + 2, sizeof(*context.stack));
    context.calls = calloc(body->count + 1, sizeof(*context.calls));
    context.snapshots = calloc(body->count + 1, sizeof(*context.snapshots));
    if (!context.body.steps || !context.stack || !context.calls || !context.snapshots) {
        fprintf(stderr, "Cannot allocate source type resolution\n"); exit(1);
    }
    memcpy(context.body.steps, body->steps, body->count * sizeof(*body->steps));
    context.controls = rpn_index_controls(body);
    resolve_range(&context, begin, end);
    if (errors == global_errors) check_resolved_range(&context, begin, end);
    free(context.controls);
    free(context.body.steps);
    free(context.stack);
    for (size_t i = 0; i < context.call_count; i++) free(context.calls[i].arguments.items);
    clear_type_snapshots(&context);
    free(context.calls);
    free(context.snapshots);
    free(context.environment.items);
    return errors == global_errors;
}
