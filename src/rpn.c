#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../include/rpn.h"

static const char *operator_text(rpn_operator_t operator) {
    static const char *const names[] = {
        "+", "-", "*", "/", ":=", "<", ">", "<=", ">=", "==", "!=", "BF", "BI", "DUP",
        "TOI", "POUT", "RET", "DROP"
    };
    size_t index = (size_t)operator;
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : NULL;
}

void rpn_init(rpn_t *rpn) {
    *rpn = (rpn_t){NULL, 0, 0};
}

void rpn_free(rpn_t *rpn) {
    free(rpn->steps);
    rpn_init(rpn);
}

size_t rpn_append_step(rpn_t *rpn, rpn_step_t step) {
    if (rpn->count == rpn->capacity) {
        size_t maximum = SIZE_MAX / sizeof(*rpn->steps);
        if (rpn->capacity > maximum / 2) {
            fprintf(stderr, "Fatal error: RPN capacity exceeded\n");
            exit(1);
        }
        size_t capacity = rpn->capacity == 0 ? 16 : rpn->capacity * 2;
        rpn_step_t *steps = realloc(rpn->steps, capacity * sizeof(*steps));
        if (steps == NULL) {
            fprintf(stderr, "Fatal error: Cannot allocate RPN steps\n");
            exit(1);
        }
        rpn->steps = steps;
        rpn->capacity = capacity;
    }
    rpn->steps[rpn->count++] = step;
    return rpn->count;
}

size_t rpn_append_symbol(rpn_t *rpn, const map_node_t *symbol) {
    if (symbol == NULL) return 0;
    return rpn_append_step(rpn, (rpn_step_t){.kind = RPN_SYMBOL, .value.symbol = symbol});
}

size_t rpn_append_destination(rpn_t *rpn, const map_node_t *symbol) {
    if (symbol == NULL) return 0;
    return rpn_append_step(rpn, (rpn_step_t){.kind = RPN_DESTINATION, .value.symbol = symbol});
}

size_t rpn_append_operator(rpn_t *rpn, rpn_operator_t operator) {
    if (operator_text(operator) == NULL) return 0;
    return rpn_append_step(rpn, (rpn_step_t){.kind = RPN_OPERATOR, .value.operator = operator});
}

size_t rpn_reserve_target(rpn_t *rpn) {
    return rpn_append_step(rpn, (rpn_step_t){.kind = RPN_TARGET, .value.target = 0});
}

bool rpn_append_sequence(rpn_t *rpn, const rpn_t *source) {
    if (rpn == source) return false;
    size_t offset = rpn->count;
    for (size_t i = 0; i < source->count; i++) {
        rpn_step_t step = source->steps[i];
        if (step.kind == RPN_TARGET && step.value.target != 0)
            step.value.target += offset;
        if (step.control_start != 0) step.control_start += offset;
        rpn_append_step(rpn, step);
    }
    return true;
}

bool rpn_patch_target(rpn_t *rpn, size_t address, size_t target) {
    if (address == 0 || address > rpn->count ||
        target == 0 || target > rpn->count + 1) return false;
    rpn_step_t *step = &rpn->steps[address - 1];
    if (step->kind != RPN_TARGET || step->value.target != 0) return false;
    step->value.target = target;
    return true;
}

bool rpn_is_complete(const rpn_t *rpn) {
    for (size_t i = 0; i < rpn->count; i++) {
        const rpn_step_t *step = &rpn->steps[i];
        if (step->kind == RPN_TARGET &&
            (step->value.target == 0 || step->value.target > rpn->count + 1))
            return false;
    }
    return true;
}

/* Quoting chains keeps their contents distinct from operators and numeric operands. */
static int print_symbol(FILE *stream, size_t address, const map_node_t *symbol) {
    if (strcmp(symbol->value, "CHAIN") != 0)
        return fprintf(stream, "%zu %s\n", address, symbol->key);

    if (fprintf(stream, "%zu \"", address) < 0) return -1;
    const unsigned char *text = (const unsigned char *)symbol->key;
    for (size_t i = 0; text[i] != '\0'; i++) {
        int result;
        switch (text[i]) {
            case '"': result = fputs("\\\"", stream); break;
            case '\\': result = fputs("\\\\", stream); break;
            case '\n': result = fputs("\\n", stream); break;
            case '\r': result = fputs("\\r", stream); break;
            case '\t': result = fputs("\\t", stream); break;
            default:
                if (text[i] < 0x20 || text[i] == 0x7f)
                    result = fprintf(stream, "\\u%04x", (unsigned int)text[i]);
                else
                    result = fputc(text[i], stream);
                break;
        }
        if (result < 0) return -1;
    }
    return fputs("\"\n", stream);
}

bool rpn_print(FILE *stream, const rpn_t *rpn) {
    if (!rpn_is_complete(rpn)) return false;
    for (size_t i = 0; i < rpn->count; i++) {
        const rpn_step_t *step = &rpn->steps[i];
        int result;
        switch (step->kind) {
            case RPN_SYMBOL:
            case RPN_DESTINATION:
                result = print_symbol(stream, i + 1, step->value.symbol);
                break;
            case RPN_CALL_FRAME:
            case RPN_CALL:
                result = fprintf(stream, "%zu %s %s", i + 1,
                    step->kind == RPN_CALL_FRAME ? "FRAME" : "CALL", step->value.symbol->key);
                if (result < 0) return false;
                if (step->receiver != NULL && fprintf(stream, " ON %s", step->receiver->key) < 0)
                    return false;
                result = fputc('\n', stream);
                break;
            case RPN_ARGUMENT_DESTINATION:
                result = fprintf(stream, "%zu ARG %s\n", i + 1, step->value.symbol->key);
                break;
            case RPN_OBJECT_VALUE:
                result = fprintf(stream, "%zu MATERIALIZE %s\n", i + 1, step->value.symbol->key);
                break;
            case RPN_ATTRIBUTE:
            case RPN_ATTRIBUTE_DESTINATION:
                result = fprintf(stream, "%zu %s%s[%s]\n", i + 1,
                    step->kind == RPN_ATTRIBUTE_DESTINATION ? "DEST " : "",
                    step->receiver->key, step->index->key);
                break;
            case RPN_OPERATOR:
                result = fprintf(stream, "%zu %s\n", i + 1, operator_text(step->value.operator));
                break;
            case RPN_TARGET:
                result = fprintf(stream, "%zu %zu\n", i + 1, step->value.target);
                break;
            default:
                return false;
        }
        if (result < 0) return false;
    }
    return !ferror(stream);
}
