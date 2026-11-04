#include <math.h>
#include "../include/numeric.h"

const char *numeric_error_message(numeric_error_t error) {
    static const char *messages[] = {"", "SINGLEF addition overflow (c)",
        "INTEGER multiplication overflow (d)", "Loss of fractional information in TOI (g)",
        "TOI value is not representable as INTEGER", "Integer division by zero"};
    return messages[error];
}
numeric_result_t numeric_integer(double number) {
    if (!isfinite(number) || number < -32768 || number > 32767)
        return (numeric_result_t){0, NUMERIC_CONVERSION};
    if (number != trunc(number)) return (numeric_result_t){0, NUMERIC_FRACTION};
    return (numeric_result_t){number, NUMERIC_OK};
}
numeric_result_t numeric_binary(double left, double right, bool floating, rpn_operator_t operator) {
    double result = 0;
    switch (operator) {
    case RPN_ADD: result = left + right; break;
    case RPN_SUBTRACT: result = left - right; break;
    case RPN_MULTIPLY: result = left * right; break;
    case RPN_DIVIDE:
        if (!floating && right == 0) return (numeric_result_t){0, NUMERIC_DIVISION};
        result = left / right;
        if (!floating) result = trunc(result);
        break;
    case RPN_LESS: result = left < right; break;
    case RPN_GREATER: result = left > right; break;
    case RPN_LESS_EQUAL: result = left <= right; break;
    case RPN_GREATER_EQUAL: result = left >= right; break;
    case RPN_EQUAL: result = left == right; break;
    case RPN_NOT_EQUAL: result = left != right; break;
    default: break;
    }
    if (operator >= RPN_LESS) return (numeric_result_t){result, NUMERIC_OK};
    if (floating) {
        if (operator == RPN_ADD && (fabs(result) >= 3.40282347e38 || fabs((float)result) >= 3.40282347e38))
            return (numeric_result_t){0, NUMERIC_FLOAT_ADD};
        return (numeric_result_t){(float)result, NUMERIC_OK};
    }
    if (operator == RPN_MULTIPLY && (result < -32768 || result > 32767))
        return (numeric_result_t){0, NUMERIC_PRODUCT};
    result = fmod(result, 65536);
    if (result < 0) result += 65536;
    if (result >= 32768) result -= 65536;
    return (numeric_result_t){result, NUMERIC_OK};
}
