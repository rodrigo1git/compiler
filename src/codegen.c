#include <stdlib.h>
#include <string.h>
#include "../include/codegen.h"
#include "../include/diagnostics.h"

static bool generation_enabled;
static rpn_t program;

void codegen_init(bool enabled) {
    generation_enabled = enabled;
    rpn_init(&program);
}

void codegen_destroy(void) {
    rpn_free(&program);
    generation_enabled = false;
}

bool codegen_print(FILE *stream) {
    return rpn_print(stream, &program);
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
        rpn_append_sequence(&program, value);
    }
    codegen_free(value);
}

static size_t append_branch(rpn_operator_t operator, int line) {
    size_t target = rpn_reserve_target(&program);
    program.steps[target - 1].source_line = line;
    rpn_append_operator(&program, operator);
    program.steps[program.count - 1].source_line = line;
    return target;
}

size_t codegen_if_begin(rpn_t *condition, int line) {
    if (condition == NULL) return 0;
    rpn_append_sequence(&program, condition);
    codegen_free(condition);
    return append_branch(RPN_BRANCH_FALSE, line);
}

size_t codegen_if_else(size_t false_target, int line) {
    if (false_target == 0) return 0;
    size_t end_target = append_branch(RPN_BRANCH, line);
    rpn_patch_target(&program, false_target, program.count + 1);
    return end_target;
}

void codegen_if_end(size_t target) {
    if (target != 0) rpn_patch_target(&program, target, program.count + 1);
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
    loop.condition_target = program.count + 1;
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
    rpn_patch_target(&program, target, loop.condition_target);
    rpn_patch_target(&program, loop.exit_target, program.count + 1);
}
