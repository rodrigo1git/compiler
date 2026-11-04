#ifndef NUMERIC_H
#define NUMERIC_H
#include "rpn.h"
typedef enum { NUMERIC_OK, NUMERIC_FLOAT_ADD, NUMERIC_PRODUCT,
               NUMERIC_FRACTION, NUMERIC_CONVERSION, NUMERIC_DIVISION } numeric_error_t;
typedef struct { double number; numeric_error_t error; } numeric_result_t;
numeric_result_t numeric_binary(double left, double right, bool floating, rpn_operator_t operator);
numeric_result_t numeric_integer(double number);
const char *numeric_error_message(numeric_error_t error);
#endif
