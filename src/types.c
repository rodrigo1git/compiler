#include <stdlib.h>
#include <string.h>
#include "../include/types.h"
#include "../include/classes.h"
#include "../include/diagnostics.h"
#include "../include/specialize.h"

type_t type_of_symbol(const map_node_t *symbol) {
    if (!symbol) {
        return (type_t){TYPE_ERROR, NULL};
    }
    if (symbol->type_symbol) {
        return (type_t){TYPE_OBJECT, symbol->type_symbol};
    }
    const char *name = symbol->declared_type ? symbol->declared_type : symbol->value;
    type_kind_t kind = TYPE_ERROR;
    if (!strcmp(name, "INTEGER")) {
        kind = TYPE_INTEGER;
    }
    if (!strcmp(name, "SINGLEF")) {
        kind = TYPE_SINGLEF;
    }
    if (!strcmp(name, "CHAIN")) {
        kind = TYPE_CHAIN;
    }
    return (type_t){kind, NULL};
}

const char *type_name(type_t type) {
    static const char *names[] = {
        "invalid",
        "INTEGER",
        "SINGLEF",
        "CHAIN",
        "object",
        "variable-position attribute",
        "comptime-dependent attribute"
    };
    return type.kind == TYPE_OBJECT ? type.class_symbol->key : names[type.kind];
}

type_t type_of_attribute(const rpn_step_t *step) {
    map_node_t *kind = step->receiver->type_symbol;
    if (step->index->usage == SYMBOL_LEXEME) {
        if (type_of_symbol(step->index).kind != TYPE_INTEGER) {
            return (type_t){TYPE_ERROR, NULL};
        }
        long position = strtol(step->index->key, NULL, 10);
        if (position < 0 || (size_t)position >= kind->class_info->effective_attributes.count) {
            return (type_t){TYPE_ERROR, NULL};
        }
        return type_of_symbol(kind->class_info->effective_attributes.items[position]);
    }
    if (step->index->is_comptime) {
        return (type_t){TYPE_COMPTIME_PENDING, kind};
    }
    type_t result = {TYPE_CANDIDATES, kind};
    if (kind->class_info->effective_attributes.count) {
        result = type_of_symbol(kind->class_info->effective_attributes.items[0]);
        for (size_t i = 1; i < kind->class_info->effective_attributes.count; i++) {
            type_t next = type_of_symbol(kind->class_info->effective_attributes.items[i]);
            if (next.kind != result.kind || next.class_symbol != result.class_symbol) {
                return (type_t){TYPE_CANDIDATES, kind};
            }
        }
    }
    return result;
}

bool types_compatible(type_t destination, type_t source) {
    if (destination.kind == TYPE_ERROR || source.kind == TYPE_ERROR ||
        destination.kind == TYPE_COMPTIME_PENDING || source.kind == TYPE_COMPTIME_PENDING) {
        return true;
    }
    if (destination.kind == TYPE_CANDIDATES) {
        const symbol_list_t *fields = &destination.class_symbol->class_info->effective_attributes;
        for (size_t i = 0; i < fields->count; i++) {
            if (!types_compatible(type_of_symbol(fields->items[i]), source)) {
                return false;
            }
        }
        return true;
    }
    if (source.kind == TYPE_CANDIDATES) {
        const symbol_list_t *fields = &source.class_symbol->class_info->effective_attributes;
        for (size_t i = 0; i < fields->count; i++) {
            if (!types_compatible(destination, type_of_symbol(fields->items[i]))) {
                return false;
            }
        }
        return true;
    }
    if (destination.kind != source.kind) {
        return false;
    }
    if (source.kind != TYPE_OBJECT) {
        return true;
    }
    const map_node_t *a = destination.class_symbol;
    const map_node_t *b = source.class_symbol;
    return a == b || (a->class_info->class_code && b->class_info->class_code &&
                      !strcmp(a->class_info->class_code, b->class_info->class_code) &&
                      class_same_layout(a, b));
}

static bool numeric(type_t type) {
    if (type.kind == TYPE_COMPTIME_PENDING || type.kind == TYPE_ERROR) {
        return true;
    }
    if (type.kind == TYPE_CANDIDATES) {
        const symbol_list_t *fields = &type.class_symbol->class_info->effective_attributes;
        for (size_t i = 0; i < fields->count; i++) {
            if (!numeric(type_of_symbol(fields->items[i]))) {
                return false;
            }
        }
        return true;
    }
    return type.kind == TYPE_INTEGER || type.kind == TYPE_SINGLEF;
}

static type_t binary_type(type_t left, type_t right, rpn_operator_t op, int line) {
    if (!numeric(left) || !numeric(right) || !types_compatible(left, right)) {
        report_semantic_error(line, "Incompatible operand types: %s and %s",
                              type_name(left), type_name(right));
        return (type_t){TYPE_ERROR, NULL};
    }
    if (op >= RPN_LESS) {
        return (type_t){TYPE_INTEGER, NULL};
    }
    if (left.kind == TYPE_COMPTIME_PENDING || right.kind == TYPE_COMPTIME_PENDING) {
        return (type_t){TYPE_COMPTIME_PENDING, NULL};
    }
    return left;
}

static type_t check_attribute_step(const rpn_step_t *step, int line) {
    if (type_of_symbol(step->index).kind != TYPE_INTEGER) {
        report_semantic_error(line, "Attribute index requires INTEGER; received %s",
                              type_name(type_of_symbol(step->index)));
    }
    if (!step->index->is_comptime && step->index->usage != SYMBOL_LEXEME) {
        const symbol_list_t *fields = &step->receiver->type_symbol->class_info->effective_attributes;
        for (size_t i = 0; i < fields->count; i++) {
            class_attribute_at(step->receiver->type_symbol, i, line);
            if (step->kind == RPN_ATTRIBUTE_DESTINATION && fields->items[i]->is_comptime) {
                report_semantic_error(line, "Writing a comptime attribute requires a compile-time index");
            }
        }
    }
    return type_of_attribute(step);
}

static void check_operator_step(const rpn_step_t *step, const map_node_t *function,
                                type_t *stack, size_t *count, int line) {
    rpn_operator_t op = step->value.operator;
    size_t needed = (op <= RPN_NOT_EQUAL || op == RPN_BRANCH_FALSE) ? 2 : 1;
    if (*count < needed) {
        report_semantic_error(line, "Incomplete intermediate operand stack during type checking");
        *count = 0;
        return;
    }
    type_t right = stack[--(*count)];
    if (op <= RPN_NOT_EQUAL) {
        type_t left = stack[--(*count)];
        if (op == RPN_ASSIGN) {
            if (!types_compatible(right, left)) {
                report_semantic_error(line, "Incompatible assignment types: %s := %s",
                                      type_name(right), type_name(left));
            }
        } else {
            stack[(*count)++] = binary_type(left, right, op, line);
        }
        return;
    }
    if (op == RPN_TO_INTEGER) {
        if (!types_compatible((type_t){TYPE_SINGLEF, NULL}, right)) {
            report_semantic_error(line, "TOI requires SINGLEF; received %s", type_name(right));
        }
        stack[(*count)++] = (type_t){TYPE_INTEGER, NULL};
        return;
    }
    if (op == RPN_DUPLICATE) {
        stack[(*count)++] = right;
        stack[(*count)++] = right;
        return;
    }
    if (op == RPN_RETURN) {
        if (right.kind == TYPE_OBJECT) {
            report_semantic_error(line, "Object returns are not allowed");
        } else if (!function || !types_compatible(type_of_symbol(function), right)) {
            report_semantic_error(line, "Incompatible return type: %s", type_name(right));
        }
        return;
    }
    if (op == RPN_OUTPUT) {
        if (!numeric(right) && right.kind != TYPE_CHAIN) {
            report_semantic_error(line, "POUT requires a numeric expression or char chain");
        }
        return;
    }
    if (op == RPN_BRANCH_FALSE) {
        type_t condition = stack[--(*count)];
        if (!types_compatible((type_t){TYPE_INTEGER, NULL}, condition)) {
            report_semantic_error(line, "Condition requires an INTEGER comparison result");
        }
    }
}

bool types_check_slice(const rpn_t *body, const map_node_t *function,
                       const rpn_t *origin, size_t begin, size_t end) {
    semantic_origin_t previous = set_semantic_origin((semantic_origin_t){origin, begin});
    int errors = global_errors;
    size_t slice_length = end - begin;
    type_t *stack = calloc(slice_length + 2, sizeof(*stack));
    if (!stack) {
        fprintf(stderr, "Cannot allocate type stack\n");
        exit(1);
    }
    size_t count = 0;
    for (size_t pc = begin; pc < end; pc++) {
        set_semantic_origin((semantic_origin_t){origin, pc});
        const rpn_step_t *step = &body->steps[pc];
        int fallback_line = function ? function->declaration_line : 1;
        int line = step->source_line ? step->source_line : fallback_line;
        switch (step->kind) {
            case RPN_SYMBOL:
            case RPN_DESTINATION:
            case RPN_ARGUMENT_DESTINATION:
                stack[count++] = type_of_symbol(step->value.symbol);
                break;
            case RPN_ATTRIBUTE:
            case RPN_ATTRIBUTE_DESTINATION:
                stack[count++] = check_attribute_step(step, line);
                break;
            case RPN_CALL:
                stack[count++] = type_of_symbol(step->value.symbol);
                break;
            case RPN_CALL_FRAME:
            case RPN_INDEX_CHECK:
            case RPN_OBJECT_VALUE:
                break;
            case RPN_TARGET:
                if (step->loop_variable && type_of_symbol(step->loop_variable).kind != TYPE_INTEGER) {
                    report_semantic_error(line, "FROM control variable requires INTEGER");
                }
                stack[count++] = (type_t){TYPE_INTEGER, NULL};
                break;
            case RPN_OPERATOR:
                check_operator_step(step, function, stack, &count, line);
                break;
        }
    }
    if (count) {
        report_semantic_error(function ? function->declaration_line : 1,
                              "Intermediate body leaves operands during type checking");
    }
    free(stack);
    set_semantic_origin(previous);
    return errors == global_errors;
}

bool types_check_body(const rpn_t *body, const map_node_t *function) {
    return types_check_slice(body, function, body, 0, body->count);
}

bool types_check_sources(const rpn_t *main_body) {
    int errors = global_errors;
    if (!class_check_containment()) {
        return false;
    }
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
