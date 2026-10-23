#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "../include/specialize.h"
#include "../include/diagnostics.h"
#include "../include/scope.h"

typedef struct {
    bool known;
    double number;
    bool floating;
    const map_node_t *constant;
    const map_node_t *object_type;
    const map_node_t *object;
    const map_node_t *object_snapshot;
    const map_node_t *destination;
    rpn_step_kind_t destination_kind;
    size_t start;
    size_t end;
    bool removable;
} value_t;

typedef struct {
    const map_node_t *symbol;
    const map_node_t *object;
    value_t value;
} binding_t;

typedef struct {
    binding_t *items;
    size_t count;
} environment_t;

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
    environment_t before;
    environment_t after;
    rpn_t body;
    value_t returned;
    bool active;
    bool unknown_entry;
    const map_node_t *receiver;
    struct function_instance *next;
} function_instance_t;

typedef struct pending_call {
    function_template_t *source;
    const map_node_t *receiver;
    environment_t arguments;
    size_t frame_step;
    int errors;
    struct pending_call *parent;
} pending_call_t;

typedef struct {
    const rpn_t *input;
    rpn_t *output;
    environment_t *environment;
    map_node_t *function;
    const map_node_t *receiver;
    value_t *stack;
    size_t count;
    pending_call_t *pending;
    bool external_unknown;
    bool stopped;
    bool has_return;
    value_t returned;
    size_t *controls;
} context_t;

static function_template_t *templates;
static function_template_t *last_template;
static function_instance_t *instances;
static function_instance_t *last_instance;
static rpn_t main_body;
static unsigned int serial;
static unsigned int depth;
static size_t evaluation_budget;

static void *resize(void *pointer, size_t size) {
    void *result = realloc(pointer, size);
    if (result == NULL && size != 0) {
        fprintf(stderr, "Fatal error: Cannot allocate compile-time evaluation state\n");
        exit(1);
    }
    return result;
}

static bool same_value(value_t left, value_t right) {
    return left.known == right.known && (!left.known ||
        (left.number == right.number && left.floating == right.floating));
}

static value_t get_value(const environment_t *environment, const map_node_t *symbol,
                         const map_node_t *object) {
    for (size_t i = environment->count; i > 0; i--)
        if (environment->items[i - 1].symbol == symbol && environment->items[i - 1].object == object)
            return environment->items[i - 1].value;
    return (value_t){0};
}

static void set_value(environment_t *environment, const map_node_t *symbol,
                      const map_node_t *object, value_t value) {
    for (size_t i = 0; i < environment->count; i++) {
        if (environment->items[i].symbol == symbol && environment->items[i].object == object) {
            environment->items[i].value = value;
            return;
        }
    }
    environment->items = resize(environment->items, (environment->count + 1) * sizeof(*environment->items));
    environment->items[environment->count++] = (binding_t){symbol, object, value};
}

static environment_t copy_environment(const environment_t *source) {
    environment_t result = {0};
    if (source->count != 0) {
        result.items = resize(NULL, source->count * sizeof(*result.items));
        memcpy(result.items, source->items, source->count * sizeof(*result.items));
        result.count = source->count;
    }
    return result;
}

static void forget_runtime(environment_t *environment) {
    for (size_t i = 0; i < environment->count; i++)
        if (!environment->items[i].symbol->is_comptime)
            environment->items[i].value = (value_t){0};
}

static void forget_call_writes(environment_t *environment, const function_template_t *source) {
    for (size_t i = 0; i < environment->count; i++) {
        binding_t *binding = &environment->items[i];
        if (binding->symbol->is_comptime) continue;
        bool written = source->writes_attributes &&
            (binding->object != NULL || binding->symbol->type_symbol != NULL);
        for (size_t j = 0; j < source->runtime_writes.count; j++)
            if (source->runtime_writes.items[j] == binding->symbol ||
                source->runtime_writes.items[j] == binding->object) written = true;
        if (written) binding->value = (value_t){0};
    }
}

static environment_t join_environments(const environment_t *left, const environment_t *right) {
    environment_t result = copy_environment(left);
    for (size_t i = 0; i < right->count; i++) {
        binding_t binding = right->items[i];
        value_t previous = get_value(left, binding.symbol, binding.object);
        set_value(&result, binding.symbol, binding.object,
            same_value(previous, binding.value) ? binding.value : (value_t){0});
    }
    for (size_t i = 0; i < result.count; i++) {
        binding_t *binding = &result.items[i];
        if (!same_value(binding->value, get_value(right, binding->symbol, binding->object)))
            binding->value = (value_t){0};
    }
    return result;
}

static const map_node_t *constant_symbol(value_t value, int line) {
    if (!value.known) return NULL;
    if (value.constant != NULL) return value.constant;
    char text[96];
    if (!isfinite(value.number) || (!value.floating && (value.number < -32768 || value.number > 32767)) ||
        (value.floating && value.number != 0.0 &&
         (fabs(value.number) <= 1.17549435e-38 || fabs(value.number) >= 3.40282347e38))) {
        report_semantic_error(line, "Compile-time result is outside the range of %s",
            value.floating ? "SINGLEF" : "INTEGER");
        return NULL;
    }
    if (value.floating) {
        snprintf(text, sizeof(text), "%.9g", (double)(float)value.number);
        char *exponent = strchr(text, 'e');
        if (exponent != NULL) *exponent = 's';
        if (strchr(text, '.') == NULL) {
            exponent = strchr(text, 's');
            if (exponent != NULL) {
                size_t offset = (size_t)(exponent - text);
                memmove(text + offset + 2, text + offset, strlen(text + offset) + 1);
                text[offset] = '.';
                text[offset + 1] = '0';
            } else strcat(text, ".0");
        }
    } else snprintf(text, sizeof(text), "%ld$i", (long)value.number);
    return map_put(symbol_table, text, value.floating ? "SINGLEF" : "INTEGER");
}

static value_t numeric_constant(const map_node_t *symbol) {
    value_t value = {0};
    if (strcmp(symbol->value, "INTEGER") != 0 && strcmp(symbol->value, "SINGLEF") != 0) return value;
    char *text = resize(NULL, strlen(symbol->key) + 1);
    strcpy(text, symbol->key);
    for (char *p = text; *p != '\0'; p++) if (*p == 's') *p = 'e';
    value.number = strtod(text, NULL);
    value.floating = strcmp(symbol->value, "SINGLEF") == 0;
    if (value.floating) value.number = (float)value.number;
    value.known = true;
    value.constant = symbol;
    free(text);
    return value;
}

static void push(context_t *context, value_t value) {
    context->stack = resize(context->stack, (context->count + 1) * sizeof(*context->stack));
    context->stack[context->count++] = value;
}

static value_t pop(context_t *context, int line) {
    if (context->count != 0) return context->stack[--context->count];
    report_semantic_error(line, "Invalid intermediate expression stack");
    return (value_t){0};
}

static size_t emit(context_t *context, rpn_step_t step) {
    return rpn_append_step(context->output, step);
}

static void discard(context_t *context, value_t value, int line) {
    if (value.removable && value.end == context->output->count)
        context->output->count = value.start;
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

static bool class_has_comptime(const map_node_t *class_symbol) {
    if (class_symbol == NULL) return false;
    for (size_t i = 0; i < class_symbol->effective_attributes.count; i++)
        if (class_symbol->effective_attributes.items[i]->is_comptime) return true;
    return false;
}

static value_t object_field(value_t object, const map_node_t *field, const environment_t *environment) {
    if (object.object_snapshot != NULL)
        for (size_t i = 0; i < object.object_snapshot->comptime_bindings.count; i++)
            if (object.object_snapshot->comptime_bindings.items[i] == field)
                return numeric_constant(object.object_snapshot->comptime_values.items[i]);
    return get_value(environment, field, object.object);
}

static void materialize_object(context_t *context, value_t *value, int line) {
    if (!class_has_comptime(value->object_type)) return;
    char name[64];
    snprintf(name, sizeof(name), "@value%u", ++serial);
    map_node_t *snapshot = map_put(symbol_table, name, "ID");
    snapshot->usage = SYMBOL_OBJECT_VALUE;
    snapshot->type_symbol = (map_node_t *)value->object_type;
    snapshot->declared_type = resize(NULL, strlen(value->object_type->key) + 1);
    strcpy(snapshot->declared_type, value->object_type->key);
    snapshot->declaration_line = line;
    for (size_t i = 0; i < value->object_type->effective_attributes.count; i++) {
        map_node_t *field = value->object_type->effective_attributes.items[i];
        if (!field->is_comptime) continue;
        value_t field_value = object_field(*value, field, context->environment);
        if (!field_value.known) {
            report_semantic_error(line, "Comptime attribute '%s' has no known value when copying an object", field->key);
            continue;
        }
        const map_node_t *constant = constant_symbol(field_value, line);
        if (constant != NULL) {
            symbol_list_append(&snapshot->comptime_bindings, field);
            symbol_list_append(&snapshot->comptime_values, (map_node_t *)constant);
        }
    }
    emit(context, (rpn_step_t){.kind = RPN_OBJECT_VALUE, .value.symbol = snapshot, .source_line = line});
    value->object_snapshot = snapshot;
}

static bool has_comptime_effects(const rpn_t *input, size_t begin, size_t end) {
    for (size_t i = begin; i < end; i++) {
        const rpn_step_t *step = &input->steps[i];
        if (step->kind == RPN_DESTINATION && step->value.symbol->is_comptime) return true;
        if (step->kind == RPN_DESTINATION && class_has_comptime(step->value.symbol->type_symbol)) return true;
        if (step->kind == RPN_ATTRIBUTE_DESTINATION && step->value.symbol != NULL &&
            step->value.symbol->is_comptime) return true;
        if (step->kind == RPN_ATTRIBUTE_DESTINATION && step->receiver != NULL)
            for (size_t j = 0; j < step->receiver->type_symbol->effective_attributes.count; j++)
                if (step->receiver->type_symbol->effective_attributes.items[j]->is_comptime) return true;
        if (step->kind == RPN_CALL) {
            function_template_t *source = find_template(step->value.symbol);
            if (source != NULL && source->external_writes) return true;
        }
    }
    return false;
}

static void transform_range(context_t *context, size_t begin, size_t end, bool unknown_control,
                            bool inspect_first_control);
static function_instance_t *instantiate(function_template_t *source, environment_t *caller,
                                         const environment_t *arguments, const map_node_t *receiver,
                                         bool unknown_entry, int line);

static void apply_effects(const function_instance_t *instance, environment_t *caller,
                          const map_node_t *receiver) {
    forget_call_writes(caller, instance->source);
    for (size_t i = 0; i < instance->after.count; i++) {
        binding_t binding = instance->after.items[i];
        if (binding.object == instance->receiver && instance->receiver != NULL) binding.object = receiver;
        if (binding.symbol->is_comptime && instance->source->external_writes &&
            binding.symbol->owner != instance->source->symbol &&
            (binding.object == NULL || binding.object->owner != instance->source->symbol))
            set_value(caller, binding.symbol, binding.object, binding.value);
        if (!binding.symbol->is_comptime && binding.value.known && binding.object != NULL &&
            binding.object->owner != instance->source->symbol)
            set_value(caller, binding.symbol, binding.object, binding.value);
    }
    for (size_t i = 0; i < instance->source->runtime_writes.count; i++) {
        const map_node_t *symbol = instance->source->runtime_writes.items[i];
        if (symbol->usage != SYMBOL_ATTRIBUTE)
            set_value(caller, symbol, NULL, get_value(&instance->after, symbol, NULL));
    }
}

static void record_return(context_t *context, value_t value) {
    if (!context->has_return) context->returned = value;
    else if (!same_value(context->returned, value)) context->returned = (value_t){0};
    context->has_return = true;
}

static size_t transform_control(context_t *context, size_t start, size_t branch, bool unknown_control) {
    const rpn_t *input = context->input;
    const rpn_step_t *target_step = &input->steps[branch];
    int line = target_step->source_line;
    size_t false_start = target_step->value.target - 1;
    bool loop = false, has_else = false;
    size_t body_end = false_start, final = false_start;
    if (false_start >= 2 && is_operator(&input->steps[false_start - 1], RPN_BRANCH) &&
        input->steps[false_start - 2].kind == RPN_TARGET) {
        size_t destination = input->steps[false_start - 2].value.target - 1;
        if (destination == start) {
            loop = true;
            body_end = false_start - 2;
        } else if (destination > false_start) {
            has_else = true;
            body_end = false_start - 2;
            final = destination;
        }
    }
    bool expand = loop && has_comptime_effects(input, branch + 2, body_end);
    size_t condition_output = context->output->count;
    transform_range(context, start, branch, unknown_control, false);
    value_t condition = pop(context, line);
    if (loop && condition.known && condition.number == 0) {
        discard(context, condition, line);
        return final;
    }
    if (expand && condition.known && !unknown_control) {
        for (;;) {
            discard(context, condition, line);
            if (condition.number == 0 || context->stopped) break;
            if (evaluation_budget-- == 0) {
                report_semantic_error(line, "Compile-time loop evaluation exceeded the resource limit");
                break;
            }
            transform_range(context, branch + 2, body_end, false, true);
            if (context->stopped) break;
            transform_range(context, start, branch, false, false);
            condition = pop(context, line);
            if (!condition.known) {
                report_semantic_error(line, "Compile-time loop control became unknown");
                transform_range(context, branch + 2, body_end, true, true);
                break;
            }
        }
        return final;
    }
    if (!loop && condition.known) {
        discard(context, condition, line);
        if (condition.number != 0) transform_range(context, branch + 2, body_end, unknown_control, true);
        else if (has_else) transform_range(context, false_start, final, unknown_control, true);
        return final;
    }
    size_t false_target = rpn_reserve_target(context->output);
    emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_BRANCH_FALSE, .source_line = line});
    environment_t original = copy_environment(context->environment);
    if (loop) forget_runtime(context->environment);
    transform_range(context, branch + 2, body_end, true, true);
    bool true_stopped = context->stopped;
    environment_t true_environment = copy_environment(context->environment);
    context->stopped = false;
    if (loop) {
        size_t back = rpn_reserve_target(context->output);
        emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_BRANCH, .source_line = line});
        rpn_patch_target(context->output, back, condition_output + 1);
        rpn_patch_target(context->output, false_target, context->output->count + 1);
    } else if (has_else) {
        size_t end_target = rpn_reserve_target(context->output);
        emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_BRANCH, .source_line = line});
        rpn_patch_target(context->output, false_target, context->output->count + 1);
        free(context->environment->items);
        *context->environment = copy_environment(&original);
        transform_range(context, false_start, final, true, true);
        context->stopped = true_stopped && context->stopped;
        rpn_patch_target(context->output, end_target, context->output->count + 1);
    } else rpn_patch_target(context->output, false_target, context->output->count + 1);
    environment_t joined = loop ? copy_environment(&original) :
        join_environments(&true_environment, has_else ? context->environment : &original);
    free(context->environment->items);
    free(original.items);
    free(true_environment.items);
    *context->environment = joined;
    if (loop) forget_runtime(context->environment);
    return final;
}

static void evaluate_binary(context_t *context, rpn_step_t step) {
    value_t right = pop(context, step.source_line), left = pop(context, step.source_line);
    value_t result = {0};
    result.start = left.start;
    result.removable = left.removable && right.removable && left.end == right.start &&
        right.end == context->output->count;
    if (left.known && right.known && left.floating == right.floating) {
        result.known = true;
        result.floating = left.floating;
        switch (step.value.operator) {
            case RPN_ADD: result.number = left.number + right.number; break;
            case RPN_SUBTRACT: result.number = left.number - right.number; break;
            case RPN_MULTIPLY: result.number = left.number * right.number; break;
            case RPN_DIVIDE:
                if (right.number == 0) result.known = false;
                else {
                    result.number = left.number / right.number;
                    if (!result.floating) result.number = trunc(result.number);
                }
                break;
            case RPN_LESS: result.number = left.number < right.number; result.floating = false; break;
            case RPN_GREATER: result.number = left.number > right.number; result.floating = false; break;
            case RPN_LESS_EQUAL: result.number = left.number <= right.number; result.floating = false; break;
            case RPN_GREATER_EQUAL: result.number = left.number >= right.number; result.floating = false; break;
            case RPN_EQUAL: result.number = left.number == right.number; result.floating = false; break;
            case RPN_NOT_EQUAL: result.number = left.number != right.number; result.floating = false; break;
            default: result.known = false; break;
        }
        if (result.floating) result.number = (float)result.number;
        if (!isfinite(result.number) || (!result.floating &&
            (result.number < -32768 || result.number > 32767))) result.known = false;
    }
    emit(context, step);
    result.end = context->output->count;
    push(context, result);
}

static void assign_value(context_t *context, value_t destination, value_t value,
                          bool unknown_control, int line) {
    const map_node_t *symbol = destination.destination;
    bool argument = destination.destination_kind == RPN_ARGUMENT_DESTINATION;
    if (symbol != NULL && symbol->is_comptime) {
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
            const map_node_t *constant = constant_symbol(value, line);
            if (constant != NULL) {
                value.constant = constant;
                if (argument && context->pending != NULL)
                    set_value(&context->pending->arguments, symbol, NULL, value);
                else set_value(context->environment, symbol, destination.object, value);
            }
        }
        discard(context, value, line);
        return;
    }
    rpn_step_t step = {.kind = destination.destination_kind, .value.symbol = symbol,
        .source_line = line, .receiver = destination.object, .index = destination.constant};
    emit(context, step);
    emit(context, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = RPN_ASSIGN, .source_line = line});
    if (argument && symbol != NULL && symbol->type_symbol != NULL &&
        context->pending != NULL && value.object_type != NULL) {
        for (size_t i = 0; i < symbol->type_symbol->effective_attributes.count; i++) {
            map_node_t *formal_field = symbol->type_symbol->effective_attributes.items[i];
            if (!formal_field->is_comptime || i >= value.object_type->effective_attributes.count) continue;
            value_t actual_field = object_field(value, value.object_type->effective_attributes.items[i],
                context->environment);
            if (!actual_field.known)
                report_semantic_error(line, "Comptime attribute '%s' has no known value in argument copy", formal_field->key);
            set_value(&context->pending->arguments, formal_field, symbol, actual_field);
        }
    }
    if (symbol != NULL && value.object_type != NULL && symbol->type_symbol != NULL &&
        value.object_type != symbol->type_symbol && value.object_type->class_code != NULL &&
        symbol->type_symbol->class_code != NULL &&
        strcmp(value.object_type->class_code, symbol->type_symbol->class_code) != 0)
        report_semantic_error(line, "Object assignment requires matching class codes (topic 24)");
    if (!argument && symbol != NULL && symbol->type_symbol != NULL) {
        size_t count = symbol->type_symbol->effective_attributes.count;
        value_t *fields = resize(NULL, count * sizeof(*fields));
        for (size_t i = 0; i < count; i++) {
            fields[i] = (value_t){0};
            if (value.object_type != NULL && value.object != NULL &&
                i < value.object_type->effective_attributes.count)
                fields[i] = object_field(value, value.object_type->effective_attributes.items[i],
                    context->environment);
        }
        for (size_t i = 0; i < count; i++) {
            map_node_t *field = symbol->type_symbol->effective_attributes.items[i];
            if (field->is_comptime && (unknown_control ||
                (context->external_unknown && symbol->owner != context->function)))
                report_semantic_error(line, "Cannot modify comptime '%s' under unknown control", field->key);
            else if (field->is_comptime && !fields[i].known)
                report_semantic_error(line, "Value for comptime '%s' is not known at compile time", field->key);
            set_value(context->environment, field, symbol, fields[i]);
        }
        free(fields);
    }
    if (!argument && destination.destination_kind == RPN_ATTRIBUTE_DESTINATION && symbol == NULL)
        for (size_t i = 0; i < context->environment->count; i++)
            if (context->environment->items[i].object == destination.object)
                context->environment->items[i].value = (value_t){0};
    if (symbol != NULL && !argument)
        set_value(context->environment, symbol, destination.object, value);
}

static void transform_range(context_t *context, size_t begin, size_t end, bool unknown_control,
                            bool inspect_first_control) {
    for (size_t pc = begin; pc < end && !context->stopped; pc++) {
        if ((pc != begin || inspect_first_control) && context->controls[pc] != 0) {
            pc = transform_control(context, pc, context->controls[pc] - 1, unknown_control) - 1;
            continue;
        }
        rpn_step_t step = context->input->steps[pc];
        value_t value = {0};
        value.start = context->output->count;
        value.removable = true;
        switch (step.kind) {
            case RPN_SYMBOL: {
                const map_node_t *symbol = step.value.symbol;
                const map_node_t *object = symbol->usage == SYMBOL_ATTRIBUTE ? context->receiver : NULL;
                value = symbol->usage == SYMBOL_LEXEME ? numeric_constant(symbol) :
                    get_value(context->environment, symbol, object);
                if (symbol->is_comptime) {
                    if (!value.known)
                        report_semantic_error(step.source_line, "Comptime '%s' has no known value", symbol->key);
                    else {
                        const map_node_t *constant = constant_symbol(value, step.source_line);
                        if (constant != NULL) step.value.symbol = constant;
                    }
                }
                value.object_type = symbol->type_symbol;
                value.object = value.object_type != NULL ? symbol : NULL;
                value.object_snapshot = NULL;
                value.start = context->output->count;
                value.removable = true;
                emit(context, step);
                materialize_object(context, &value, step.source_line);
                value.end = context->output->count;
                push(context, value);
                break;
            }
            case RPN_DESTINATION:
            case RPN_ARGUMENT_DESTINATION:
                value.destination = step.value.symbol;
                value.destination_kind = step.kind;
                value.object = step.value.symbol->usage == SYMBOL_ATTRIBUTE ? context->receiver : NULL;
                push(context, value);
                break;
            case RPN_ATTRIBUTE:
            case RPN_ATTRIBUTE_DESTINATION: {
                const map_node_t *object = step.receiver;
                const map_node_t *index = step.index;
                value_t selected = index->usage == SYMBOL_LEXEME ? numeric_constant(index) :
                    get_value(context->environment, index, NULL);
                if (index->is_comptime) {
                    if (!selected.known)
                        report_semantic_error(step.source_line, "Comptime index '%s' has no known value", index->key);
                    else step.index = constant_symbol(selected, step.source_line);
                }
                map_node_t *attribute = NULL;
                if (selected.known && !selected.floating) {
                    if (selected.number < 0)
                        report_semantic_error(step.source_line, "Attribute position %ld does not exist in class '%s'",
                            (long)selected.number, object->type_symbol->key);
                    else attribute = scope_attribute_at(object->type_symbol, (size_t)selected.number, step.source_line);
                }
                if (attribute == NULL && !selected.known)
                    for (size_t i = 0; i < object->type_symbol->effective_attributes.count; i++)
                        if (object->type_symbol->effective_attributes.items[i]->is_comptime) {
                            report_semantic_error(step.source_line,
                                "Attribute position must be known when accessing comptime attributes");
                            break;
                        }
                if (step.kind == RPN_ATTRIBUTE_DESTINATION) {
                    value.destination = attribute;
                    value.destination_kind = step.kind;
                    value.constant = step.index;
                    value.object = object;
                    push(context, value);
                } else {
                    if (attribute != NULL) value = get_value(context->environment, attribute, object);
                    if (attribute != NULL && attribute->is_comptime) {
                        if (!value.known)
                            report_semantic_error(step.source_line, "Comptime attribute '%s' has no known value", attribute->key);
                        else {
                            step.kind = RPN_SYMBOL;
                            step.value.symbol = constant_symbol(value, step.source_line);
                        }
                    }
                    value.object_type = attribute != NULL ? attribute->type_symbol : NULL;
                    value.start = context->output->count;
                    value.removable = true;
                    emit(context, step);
                    materialize_object(context, &value, step.source_line);
                    value.end = context->output->count;
                    push(context, value);
                }
                break;
            }
            case RPN_CALL_FRAME: {
                pending_call_t *pending = resize(NULL, sizeof(*pending));
                *pending = (pending_call_t){.source = find_template(step.value.symbol),
                    .receiver = step.receiver != NULL ? step.receiver : context->receiver,
                    .frame_step = context->output->count, .errors = global_errors, .parent = context->pending};
                context->pending = pending;
                emit(context, step);
                break;
            }
            case RPN_CALL: {
                pending_call_t *pending = context->pending;
                if (pending == NULL || pending->source == NULL) {
                    report_semantic_error(step.source_line, "Cannot resolve intermediate call");
                    break;
                }
                function_instance_t *instance = NULL;
                if (global_errors == pending->errors)
                    instance = instantiate(pending->source, context->environment,
                        &pending->arguments, pending->receiver,
                        unknown_control || context->external_unknown, step.source_line);
                if (instance != NULL) {
                    context->output->steps[pending->frame_step].value.symbol = instance->symbol;
                    step.value.symbol = instance->symbol;
                    value = instance->returned;
                    apply_effects(instance, context->environment, pending->receiver);
                }
                emit(context, step);
                value.start = pending->frame_step;
                value.end = context->output->count;
                value.removable = false;
                push(context, value);
                if (instance == NULL) forget_call_writes(context->environment, pending->source);
                context->pending = pending->parent;
                free(pending->arguments.items);
                free(pending);
                break;
            }
            case RPN_OPERATOR:
                if (step.value.operator <= RPN_NOT_EQUAL && step.value.operator != RPN_ASSIGN)
                    evaluate_binary(context, step);
                else if (step.value.operator == RPN_ASSIGN) {
                    value_t destination = pop(context, step.source_line);
                    value_t assigned = pop(context, step.source_line);
                    assign_value(context, destination, assigned, unknown_control, step.source_line);
                } else if (step.value.operator == RPN_DUPLICATE) {
                    value = pop(context, step.source_line);
                    push(context, value);
                    value.start = context->output->count;
                    emit(context, step);
                    value.end = context->output->count;
                    value.removable = true;
                    push(context, value);
                } else if (step.value.operator == RPN_TO_INTEGER) {
                    value = pop(context, step.source_line);
                    value.constant = NULL;
                    if (value.known) {
                        value.number = trunc(value.number);
                        value.floating = false;
                        if (value.number < -32768 || value.number > 32767) value.known = false;
                    }
                    value.removable = value.removable && value.end == context->output->count;
                    emit(context, step);
                    value.end = context->output->count;
                    push(context, value);
                } else if (step.value.operator == RPN_RETURN || step.value.operator == RPN_OUTPUT) {
                    value = pop(context, step.source_line);
                    if (step.value.operator == RPN_RETURN) {
                        if (value.object_type != NULL)
                            report_semantic_error(step.source_line, "Object returns are not allowed");
                        record_return(context, value);
                        context->stopped = true;
                    }
                    emit(context, step);
                } else if (step.value.operator == RPN_DROP) {
                    pop(context, step.source_line);
                    emit(context, step);
                } else report_semantic_error(step.source_line, "Unstructured branch in intermediate code");
                break;
            case RPN_TARGET:
                report_semantic_error(step.source_line, "Unexpected standalone jump target");
                break;
            case RPN_OBJECT_VALUE:
                report_semantic_error(step.source_line, "Unexpected materialized object in a source template");
                break;
        }
    }
}

static void transform(const rpn_t *input, rpn_t *output, environment_t *environment,
                       map_node_t *function, const map_node_t *receiver, bool external_unknown,
                       value_t *returned) {
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

static bool matches_environment(const function_instance_t *instance, const environment_t *environment,
                                const map_node_t *receiver, bool unknown_entry) {
    if (instance->receiver != receiver) return false;
    if (instance->unknown_entry != unknown_entry && instance->source->external_writes) return false;
    for (size_t i = 0; i < instance->before.count; i++) {
        binding_t binding = instance->before.items[i];
        if (binding.symbol->is_comptime && !same_value(binding.value,
            get_value(environment, binding.symbol, binding.object))) return false;
    }
    for (size_t i = 0; i < environment->count; i++) {
        binding_t binding = environment->items[i];
        if (binding.symbol->is_comptime && !same_value(binding.value,
            get_value(&instance->before, binding.symbol, binding.object))) return false;
    }
    return true;
}

static function_instance_t *instantiate(function_template_t *source, environment_t *caller,
                                         const environment_t *arguments, const map_node_t *receiver,
                                         bool unknown_entry, int line) {
    environment_t environment = copy_environment(caller);
    forget_runtime(&environment);
    for (size_t i = 0; i < environment.count; i++)
        if (environment.items[i].object != NULL && environment.items[i].object->owner == source->symbol)
            environment.items[i].value = (value_t){0};
    for (int i = 0; i < symbol_table->capacity; i++)
        for (map_node_t *symbol = symbol_table->buckets[i]; symbol != NULL; symbol = symbol->next)
            if (symbol->owner == source->symbol)
                set_value(&environment, symbol, NULL, (value_t){0});
    for (size_t i = 0; i < source->symbol->parameters.count; i++) {
        map_node_t *parameter = source->symbol->parameters.items[i];
        if (!parameter->is_comptime) continue;
        value_t argument = get_value(arguments, parameter, NULL);
        if (!argument.known)
            report_semantic_error(line, "Argument for comptime parameter '%s' is not known at compile time", parameter->key);
        set_value(&environment, parameter, NULL, argument);
    }
    for (size_t i = 0; i < arguments->count; i++) {
        binding_t binding = arguments->items[i];
        if (binding.object != NULL && binding.object->owner == source->symbol)
            set_value(&environment, binding.symbol, binding.object, binding.value);
    }
    source->used = true;
    for (function_instance_t *instance = instances; instance != NULL; instance = instance->next)
        if (instance->source == source && (!source->contextual ||
            matches_environment(instance, &environment, receiver, unknown_entry))) {
            free(environment.items);
            return instance;
        }
    if (++depth > 128) {
        report_semantic_error(line, "Compile-time specialization exceeded the recursion resource limit");
        depth--;
        free(environment.items);
        return NULL;
    }
    function_instance_t *instance = resize(NULL, sizeof(*instance));
    *instance = (function_instance_t){.source = source, .symbol = source->symbol,
        .before = copy_environment(&environment), .active = true, .unknown_entry = unknown_entry,
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
    transform(source->body, &instance->body, &environment, source->symbol, receiver,
        unknown_entry, &instance->returned);
    instance->active = false;
    instance->after = environment;
    for (size_t i = 0; i < environment.count; i++) {
        binding_t binding = environment.items[i];
        if (!binding.symbol->is_comptime || !binding.value.known ||
            binding.symbol->owner != source->symbol) continue;
        const map_node_t *constant = constant_symbol(binding.value, binding.symbol->declaration_line);
        if (constant == NULL) continue;
        symbol_list_append(&instance->symbol->comptime_bindings, (map_node_t *)binding.symbol);
        symbol_list_append(&instance->symbol->comptime_values, (map_node_t *)constant);
        if (!source->contextual) ((map_node_t *)binding.symbol)->comptime_value = (map_node_t *)constant;
    }
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

bool specialize_program(const rpn_t *program) {
    for (function_template_t *source = templates; source != NULL; source = source->next) {
        for (size_t i = 0; i < source->symbol->parameters.count; i++)
            if (source->symbol->parameters.items[i]->is_comptime) source->contextual = true;
        for (size_t i = 0; i < source->body->count; i++) {
            const rpn_step_t *step = &source->body->steps[i];
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
    for (function_template_t *source = templates; source != NULL; source = source->next)
        source->symbol->comptime_template = source->contextual;
    environment_t environment = {0};
    transform(program, &main_body, &environment, NULL, NULL, false, NULL);
    for (function_template_t *source = templates; source != NULL; source = source->next) {
        if (!source->used && !source->contextual) {
            environment_t empty = {0};
            instantiate(source, &empty, &empty, NULL, false, source->symbol->declaration_line);
        }
    }
    for (size_t i = 0; i < environment.count; i++) {
        binding_t binding = environment.items[i];
        if (binding.symbol->is_comptime && binding.symbol->owner == NULL && binding.value.known)
            ((map_node_t *)binding.symbol)->comptime_value = (map_node_t *)constant_symbol(binding.value,
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
    return true;
}

void specialize_destroy(void) {
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
