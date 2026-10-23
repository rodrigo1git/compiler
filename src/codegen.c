#include <stdlib.h>
#include <string.h>
#include "../include/codegen.h"
#include "../include/diagnostics.h"
#include "../include/scope.h"
#include "../include/specialize.h"

static bool generation_enabled;
static rpn_t program;
static rpn_t *current_sequence;

struct codegen_function {
    map_node_t *symbol;
    rpn_t body;
    bool active;
    struct codegen_function *parent;
    struct codegen_function *next;
};

static codegen_function_t *functions;
static codegen_function_t *last_function;
static codegen_function_t *current_function;

void codegen_init(bool enabled) {
    (void)enabled;
    generation_enabled = true;
    specialize_init();
    rpn_init(&program);
    current_sequence = &program;
    functions = NULL;
    last_function = NULL;
    current_function = NULL;
}

void codegen_destroy(void) {
    specialize_destroy();
    rpn_free(&program);
    while (functions != NULL) {
        codegen_function_t *next = functions->next;
        rpn_free(&functions->body);
        free(functions);
        functions = next;
    }
    last_function = NULL;
    current_function = NULL;
    current_sequence = &program;
    generation_enabled = false;
}

bool codegen_print(FILE *stream) {
    return specialize_print(stream);
}

bool codegen_finalize(void) {
    return specialize_program(&program);
}

static rpn_t *new_fragment(void) {
    rpn_t *fragment = malloc(sizeof(*fragment));
    if (fragment == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate expression fragment\n");
        exit(1);
    }
    rpn_init(fragment);
    return fragment;
}

struct codegen_arguments {
    rpn_t **items;
    size_t count;
};

struct codegen_order {
    symbol_list_t constants;
};

struct codegen_call {
    map_node_t *function;
    map_node_t *receiver;
    codegen_arguments_t *arguments;
    int line;
};

static void *checked_realloc(void *memory, size_t size) {
    void *result = realloc(memory, size);
    if (result == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate call information\n");
        exit(1);
    }
    return result;
}

codegen_arguments_t *codegen_arguments_append(codegen_arguments_t *arguments, rpn_t *value) {
    if (arguments == NULL) {
        arguments = checked_realloc(NULL, sizeof(*arguments));
        *arguments = (codegen_arguments_t){0};
    }
    arguments->items = checked_realloc(arguments->items,
        (arguments->count + 1) * sizeof(*arguments->items));
    arguments->items[arguments->count++] = value;
    return arguments;
}

void codegen_arguments_free(codegen_arguments_t *arguments) {
    if (arguments == NULL) return;
    for (size_t i = 0; i < arguments->count; i++) codegen_free(arguments->items[i]);
    free(arguments->items);
    free(arguments);
}

codegen_order_t *codegen_order_append(codegen_order_t *order, map_node_t *constant) {
    if (order == NULL) {
        order = checked_realloc(NULL, sizeof(*order));
        *order = (codegen_order_t){0};
    }
    /* Preserve malformed constants as positions, so counts do not change on recovery. */
    order->constants.items = checked_realloc(order->constants.items,
        (order->constants.count + 1) * sizeof(*order->constants.items));
    order->constants.items[order->constants.count++] = constant;
    return order;
}

void codegen_order_free(codegen_order_t *order) {
    if (order == NULL) return;
    free(order->constants.items);
    free(order);
}

codegen_call_t *codegen_call_begin(map_node_t *function, map_node_t *receiver,
                                 codegen_arguments_t *arguments, int line) {
    codegen_call_t *call = checked_realloc(NULL, sizeof(*call));
    *call = (codegen_call_t){function, receiver, arguments, line};
    if (function != NULL && arguments != NULL &&
        arguments->count != function->parameters.count)
        report_semantic_error(line, "Call to '%s' has %zu arguments; expected %zu",
            function->key, arguments->count, function->parameters.count);
    return call;
}

void codegen_call_free(codegen_call_t *call) {
    if (call == NULL) return;
    codegen_arguments_free(call->arguments);
    free(call);
}

rpn_t *codegen_call_end(codegen_call_t *call, codegen_order_t *order) {
    rpn_t *result = NULL;
    if (call == NULL || order == NULL || call->function == NULL || call->arguments == NULL)
        goto finish;
    size_t count = call->function->parameters.count;
    if (order->constants.count != count) {
        report_semantic_error(call->line, "Evaluation order for '%s' has %zu entries; expected %zu",
            call->function->key, order->constants.count, count);
        goto finish;
    }
    if (call->arguments->count != count) goto finish;
    size_t *positions = checked_realloc(NULL, count * sizeof(*positions));
    bool specified = true;
    for (size_t i = 0; i < count; i++) {
        map_node_t *constant = order->constants.items[i];
        if (constant == NULL) { specified = false; break; }
        if (strcmp(constant->value, "INTEGER") != 0) {
            report_semantic_error(call->line, "Evaluation order positions must be INTEGER constants");
            specified = false; break;
        }
        long position = strtol(constant->key, NULL, 10);
        if (position < 1 || (size_t)position > count) {
            report_semantic_error(call->line, "Evaluation order position %ld is outside 1..%zu", position, count);
            specified = false; break;
        }
        positions[i] = (size_t)position - 1;
        for (size_t j = 0; j < i; j++)
            if (positions[j] == positions[i]) {
                report_semantic_error(call->line, "Evaluation order repeats position %ld", position);
                specified = false;
            }
    }
    if (!specified) {
        free(positions);
        goto finish;
    }
    if (!generation_enabled) { free(positions); goto finish; }
    for (size_t i = 0; i < count; i++)
        if (call->arguments->items[i] == NULL) { free(positions); goto finish; }
    result = new_fragment();
    /* TP3 pp.5,7: evaluate and copy in the explicit order, retaining formal positions.
       A pending frame keeps nested/recursive calls independent of the caller's frame. */
    rpn_append_step(result, (rpn_step_t){.kind = RPN_CALL_FRAME,
        .value.symbol = call->function, .receiver = call->receiver, .source_line = call->line});
    for (size_t i = 0; i < count; i++) {
        size_t position = positions[i];
        rpn_append_sequence(result, call->arguments->items[position]);
        rpn_append_step(result, (rpn_step_t){.kind = RPN_ARGUMENT_DESTINATION,
            .value.symbol = call->function->parameters.items[position], .source_line = call->line});
        rpn_append_operator(result, RPN_ASSIGN);
        result->steps[result->count - 1].source_line = call->line;
    }
    rpn_append_step(result, (rpn_step_t){.kind = RPN_CALL,
        .value.symbol = call->function, .receiver = call->receiver, .source_line = call->line});
    free(positions);
finish:
    codegen_call_free(call);
    codegen_order_free(order);
    return result;
}

codegen_attribute_t codegen_attribute(map_node_t *object, map_node_t *index, int line) {
    codegen_attribute_t attribute = {object, index, line};
    if (object == NULL || index == NULL) return (codegen_attribute_t){0};
    map_node_t *class_symbol = object->type_symbol;
    if (!scope_class_layout(class_symbol, line)) return (codegen_attribute_t){0};
    if (strcmp(index->value, "INTEGER") == 0) {
        long position = strtol(index->key, NULL, 10);
        if (position < 0 || (size_t)position >= class_symbol->effective_attributes.count) {
            report_semantic_error(line, "Attribute position '%s' does not exist in class '%s'",
                index->key, class_symbol->key);
            return (codegen_attribute_t){0};
        }
        if (scope_attribute_at(class_symbol, (size_t)position, line) == NULL)
            return (codegen_attribute_t){0};
    }
    return attribute;
}

rpn_t *codegen_attribute_value(codegen_attribute_t attribute) {
    if (!generation_enabled || attribute.object == NULL || attribute.index == NULL) return NULL;
    rpn_t *value = new_fragment();
    map_node_t *member = NULL;
    if (strcmp(attribute.index->value, "INTEGER") == 0) {
        long position = strtol(attribute.index->key, NULL, 10);
        if (position >= 0) member = scope_attribute_at(attribute.object->type_symbol, (size_t)position, attribute.line);
    }
    rpn_append_step(value, (rpn_step_t){.kind = RPN_ATTRIBUTE, .value.symbol = member,
        .receiver = attribute.object, .index = attribute.index, .source_line = attribute.line});
    return value;
}

void codegen_attribute_assignment(codegen_attribute_t attribute, rpn_t *value) {
    rpn_t *destination = codegen_attribute_value(attribute);
    if (destination != NULL && value != NULL) {
        destination->steps[0].kind = RPN_ATTRIBUTE_DESTINATION;
        rpn_append_sequence(value, destination);
        rpn_append_operator(value, RPN_ASSIGN);
        value->steps[value->count - 1].source_line = attribute.line;
        rpn_append_sequence(current_sequence, value);
    }
    codegen_free(destination);
    codegen_free(value);
}

void codegen_class_end(map_node_t *class_symbol, int line) {
    scope_class_complete(class_symbol, line);
}

void codegen_free(rpn_t *fragment) {
    if (fragment == NULL) return;
    rpn_free(fragment);
    free(fragment);
}

rpn_t *codegen_value(map_node_t *symbol, int line) {
    if (!generation_enabled || symbol == NULL) return NULL;
    rpn_t *fragment = new_fragment();
    rpn_append_symbol(fragment, symbol);
    fragment->steps[0].source_line = line;
    return fragment;
}

rpn_t *codegen_unary(rpn_t *value, rpn_operator_t operator, int line) {
    if (value == NULL) return NULL;
    rpn_append_operator(value, operator);
    value->steps[value->count - 1].source_line = line;
    return value;
}

rpn_t *codegen_binary(rpn_t *left, rpn_t *right,
                         rpn_operator_t operator, int line) {
    if (left == NULL || right == NULL) {
        codegen_free(left);
        codegen_free(right);
        return NULL;
    }
    rpn_append_sequence(left, right);
    codegen_free(right);
    rpn_append_operator(left, operator);
    left->steps[left->count - 1].source_line = line;
    return left;
}

rpn_t *codegen_target(rpn_t *targets, map_node_t *symbol, int line) {
    if (!generation_enabled || symbol == NULL) {
        codegen_free(targets);
        return NULL;
    }
    if (targets == NULL) targets = new_fragment();
    rpn_append_destination(targets, symbol);
    targets->steps[targets->count - 1].source_line = line;
    return targets;
}

rpn_t *codegen_assignment_expression(rpn_t *targets, rpn_t *value) {
    if (targets == NULL || value == NULL) {
        codegen_free(targets);
        codegen_free(value);
        return NULL;
    }
    /* Assign right to left while retaining a value for the enclosing expression. */
    for (size_t i = targets->count; i > 0; i--) {
        rpn_step_t target = targets->steps[i - 1];
        rpn_append_operator(value, RPN_DUPLICATE);
        value->steps[value->count - 1].source_line = target.source_line;
        rpn_append_destination(value, target.value.symbol);
        value->steps[value->count - 1].source_line = target.source_line;
        rpn_append_operator(value, RPN_ASSIGN);
        value->steps[value->count - 1].source_line = target.source_line;
    }
    codegen_free(targets);
    return value;
}

void codegen_assignment(map_node_t *target, rpn_t *value, int line) {
    /* Topic 24 is a class-code rule, not the general numeric compatibility pass. */
    if (target != NULL && target->type_symbol != NULL && value != NULL && value->count == 1 &&
        value->steps[0].kind == RPN_SYMBOL) {
        const map_node_t *source = value->steps[0].value.symbol;
        if (source->type_symbol != NULL && source->type_symbol != target->type_symbol &&
            source->type_symbol->class_code != NULL && target->type_symbol->class_code != NULL &&
            strcmp(source->type_symbol->class_code, target->type_symbol->class_code) != 0) {
            report_semantic_error(line, "Object assignment requires matching class codes (topic 24)");
            codegen_free(value);
            return;
        }
    }
    if (target != NULL && value != NULL) {
        rpn_append_destination(value, target);
        value->steps[value->count - 1].source_line = line;
        rpn_append_operator(value, RPN_ASSIGN);
        value->steps[value->count - 1].source_line = line;
        rpn_append_sequence(current_sequence, value);
    }
    codegen_free(value);
}

void codegen_output(rpn_t *value, int line) {
    value = codegen_unary(value, RPN_OUTPUT, line);
    if (value == NULL) return;
    rpn_append_sequence(current_sequence, value);
    codegen_free(value);
}

static bool returns_object(const rpn_t *expression) {
    const map_node_t **types = calloc(expression->count + 1, sizeof(*types));
    if (types == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate return category information\n");
        exit(1);
    }
    size_t count = 0;
    for (size_t i = 0; i < expression->count; i++) {
        const rpn_step_t *step = &expression->steps[i];
        switch (step->kind) {
            case RPN_SYMBOL:
            case RPN_CALL:
                types[count++] = step->value.symbol->type_symbol;
                break;
            case RPN_ATTRIBUTE:
                types[count++] = step->value.symbol != NULL ? step->value.symbol->type_symbol : NULL;
                break;
            case RPN_DESTINATION:
            case RPN_ARGUMENT_DESTINATION:
            case RPN_ATTRIBUTE_DESTINATION:
                types[count++] = NULL;
                break;
            case RPN_OPERATOR:
                if (step->value.operator == RPN_ASSIGN && count >= 2) count -= 2;
                else if (step->value.operator == RPN_DUPLICATE && count != 0) {
                    types[count] = types[count - 1];
                    count++;
                } else if (step->value.operator == RPN_TO_INTEGER && count != 0) types[count - 1] = NULL;
                else if (step->value.operator <= RPN_NOT_EQUAL && count >= 2) types[--count - 1] = NULL;
                break;
            default: break;
        }
    }
    bool result = count != 0 && types[count - 1] != NULL;
    free(types);
    return result;
}

void codegen_return(rpn_t *value, int line) {
    if (current_function == NULL) {
        codegen_free(value);
        return;
    }
    if (value != NULL && returns_object(value)) {
        report_semantic_error(line, "Object returns are not allowed");
        codegen_free(value);
        return;
    }
    value = codegen_unary(value, RPN_RETURN, line);
    if (value == NULL) return;
    rpn_append_sequence(current_sequence, value);
    codegen_free(value);
}

codegen_function_t *codegen_function_begin(map_node_t *symbol, int line) {
    if (!generation_enabled || symbol == NULL) return NULL;
    if (symbol->declared_type == NULL ||
        (strcmp(symbol->declared_type, "INTEGER") != 0 &&
         strcmp(symbol->declared_type, "SINGLEF") != 0)) {
        report_semantic_error(line, "Functions and methods must return INTEGER or SINGLEF; object returns are not allowed");
        return NULL;
    }
    codegen_function_t *function = malloc(sizeof(*function));
    if (function == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate function code\n");
        exit(1);
    }
    *function = (codegen_function_t){.symbol = symbol, .active = true,
                                     .parent = current_function};
    rpn_init(&function->body);
    specialize_register(symbol, &function->body);
    if (last_function == NULL) functions = function;
    else last_function->next = function;
    last_function = function;
    current_function = function;
    current_sequence = &function->body;
    return function;
}

void codegen_function_abort(codegen_function_t *function) {
    if (function == NULL || !function->active) return;
    while (current_function != NULL) {
        codegen_function_t *closed = current_function;
        current_function = closed->parent;
        closed->active = false;
        if (closed == function) break;
    }
    current_sequence = current_function != NULL ? &current_function->body : &program;
}

void codegen_function_end(codegen_function_t *function) {
    if (function == NULL || !function->active) return;
    bool floating = strcmp(function->symbol->declared_type, "SINGLEF") == 0;
    map_node_t *zero = map_put(symbol_table, floating ? "0.0" : "0$i",
                               floating ? "SINGLEF" : "INTEGER");
    /* Falling through a function returns zero; explicit RET steps exit earlier. */
    codegen_return(codegen_value(zero, function->symbol->declaration_line),
                   function->symbol->declaration_line);
    codegen_function_abort(function);
}

static size_t append_branch(rpn_operator_t operator, int line) {
    size_t target = rpn_reserve_target(current_sequence);
    current_sequence->steps[target - 1].source_line = line;
    rpn_append_operator(current_sequence, operator);
    current_sequence->steps[current_sequence->count - 1].source_line = line;
    return target;
}

size_t codegen_if_begin(rpn_t *condition, int line) {
    if (condition == NULL) return 0;
    size_t start = current_sequence->count + 1;
    rpn_append_sequence(current_sequence, condition);
    codegen_free(condition);
    size_t target = append_branch(RPN_BRANCH_FALSE, line);
    current_sequence->steps[target - 1].control_start = start;
    return target;
}

size_t codegen_if_else(size_t false_target, int line) {
    if (false_target == 0) return 0;
    size_t end_target = append_branch(RPN_BRANCH, line);
    rpn_patch_target(current_sequence, false_target, current_sequence->count + 1);
    return end_target;
}

void codegen_if_end(size_t target) {
    if (target != 0) rpn_patch_target(current_sequence, target, current_sequence->count + 1);
}

static double constant_value(const map_node_t *symbol) {
    char *text = malloc(strlen(symbol->key) + 1);
    if (text == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate numeric constant text\n");
        exit(1);
    }
    strcpy(text, symbol->key);
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == 's') text[i] = 'e';
    }
    double value = strtod(text, NULL);
    free(text);
    return value;
}

codegen_loop_t codegen_for_begin(map_node_t *variable, map_node_t *initial,
                                map_node_t *final, map_node_t *step, int line) {
    codegen_loop_t loop = {0};
    if (!generation_enabled || variable == NULL || initial == NULL ||
        final == NULL || step == NULL) return loop;

    bool ascending = constant_value(initial) <= constant_value(final);
    loop.variable = variable;
    loop.step = step;
    loop.update_operator = ascending ? RPN_ADD : RPN_SUBTRACT;
    loop.source_line = line;
    codegen_assignment(variable, codegen_value(initial, line), line);
    loop.condition_target = current_sequence->count + 1;
    rpn_t *condition = codegen_binary(codegen_value(variable, line),
                                     codegen_value(final, line),
                                     ascending ? RPN_LESS : RPN_GREATER, line);
    loop.exit_target = codegen_if_begin(condition, line);
    return loop;
}

void codegen_for_end(codegen_loop_t loop) {
    if (loop.exit_target == 0) return;
    rpn_t *update = codegen_binary(codegen_value(loop.variable, loop.source_line),
                                  codegen_value(loop.step, loop.source_line),
                                  loop.update_operator, loop.source_line);
    codegen_assignment(loop.variable, update, loop.source_line);
    size_t target = append_branch(RPN_BRANCH, loop.source_line);
    rpn_patch_target(current_sequence, target, loop.condition_target);
    rpn_patch_target(current_sequence, loop.exit_target, current_sequence->count + 1);
}
