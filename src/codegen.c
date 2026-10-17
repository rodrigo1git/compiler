#include <stdlib.h>
#include <string.h>
#include "../include/codegen.h"
#include "../include/diagnostics.h"

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
    generation_enabled = enabled;
    rpn_init(&program);
    current_sequence = &program;
    functions = NULL;
    last_function = NULL;
    current_function = NULL;
}

void codegen_destroy(void) {
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
    if (!rpn_is_complete(&program)) return false;
    for (codegen_function_t *function = functions; function != NULL; function = function->next) {
        if (function->active || !rpn_is_complete(&function->body)) return false;
    }
    if (!rpn_print(stream, &program)) return false;
    for (codegen_function_t *function = functions; function != NULL; function = function->next) {
        if (fprintf(stream, "\n--- FUNCTION %s ---\n", function->symbol->key) < 0 ||
            !rpn_print(stream, &function->body)) return false;
    }
    return true;
}

void codegen_unsupported(int line, const char *feature) {
    if (generation_enabled)
        report_semantic_error(line, "RPN generation does not support %s yet", feature);
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

void codegen_return(rpn_t *value, int line) {
    if (current_function == NULL) {
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
        codegen_unsupported(line, "class-valued function returns");
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
    rpn_append_sequence(current_sequence, condition);
    codegen_free(condition);
    return append_branch(RPN_BRANCH_FALSE, line);
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
