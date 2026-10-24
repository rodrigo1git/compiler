#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "../include/comptime_values.h"
#include "../include/diagnostics.h"

static void *resize(void *pointer, size_t size) {
    void *result = realloc(pointer, size);
    if (result == NULL && size != 0) {
        fprintf(stderr, "Fatal error: Cannot allocate compile-time evaluation state\n");
        exit(1);
    }
    return result;
}

bool comptime_values_equal(comptime_value_t left, comptime_value_t right) {
    return left.known == right.known && (!left.known ||
        (left.number == right.number && left.floating == right.floating));
}

comptime_value_t comptime_get(const comptime_environment_t *environment, const map_node_t *symbol,
                             const map_node_t *object) {
    for (size_t i = environment->count; i > 0; i--)
        if (environment->items[i - 1].symbol == symbol && environment->items[i - 1].object == object)
            return environment->items[i - 1].value;
    return (comptime_value_t){0};
}

void comptime_set(comptime_environment_t *environment, const map_node_t *symbol,
                  const map_node_t *object, comptime_value_t value) {
    for (size_t i = 0; i < environment->count; i++) {
        if (environment->items[i].symbol == symbol && environment->items[i].object == object) {
            environment->items[i].value = value;
            return;
        }
    }
    environment->items = resize(environment->items, (environment->count + 1) * sizeof(*environment->items));
    environment->items[environment->count++] = (comptime_binding_t){symbol, object, value};
}

comptime_environment_t comptime_copy_environment(const comptime_environment_t *source) {
    comptime_environment_t result = {0};
    if (source->count != 0) {
        result.items = resize(NULL, source->count * sizeof(*result.items));
        memcpy(result.items, source->items, source->count * sizeof(*result.items));
        result.count = source->count;
    }
    return result;
}

void comptime_forget_runtime(comptime_environment_t *environment) {
    for (size_t i = 0; i < environment->count; i++)
        if (!environment->items[i].symbol->is_comptime)
            environment->items[i].value = (comptime_value_t){0};
}

comptime_environment_t comptime_join_environments(const comptime_environment_t *left,
                                                  const comptime_environment_t *right) {
    comptime_environment_t result = comptime_copy_environment(left);
    for (size_t i = 0; i < right->count; i++) {
        comptime_binding_t binding = right->items[i];
        comptime_value_t previous = comptime_get(left, binding.symbol, binding.object);
        comptime_set(&result, binding.symbol, binding.object,
            comptime_values_equal(previous, binding.value) ? binding.value : (comptime_value_t){0});
    }
    for (size_t i = 0; i < result.count; i++) {
        comptime_binding_t *binding = &result.items[i];
        if (!comptime_values_equal(binding->value, comptime_get(right, binding->symbol, binding->object)))
            binding->value = (comptime_value_t){0};
    }
    return result;
}

const map_node_t *comptime_make_constant(comptime_value_t value, int line) {
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

comptime_value_t comptime_read_constant(const map_node_t *symbol) {
    comptime_value_t value = {0};
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

comptime_value_t comptime_binary(comptime_value_t left, comptime_value_t right, rpn_operator_t operator) {
    comptime_value_t result = {0};
    if (left.known && right.known && left.floating == right.floating) {
        result.known = true;
        result.floating = left.floating;
        switch (operator) {
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
    return result;
}

comptime_value_t comptime_to_integer(comptime_value_t value) {
    value.constant = NULL;
    if (value.known) {
        value.number = trunc(value.number);
        value.floating = false;
        if (value.number < -32768 || value.number > 32767) value.known = false;
    }
    return value;
}
