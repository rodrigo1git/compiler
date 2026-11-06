# Lexer matrices

Generated from `src/transition_table.c` and `src/semantic_actions.c`.

`F_CONS` consumes the current character; `F_RET` preserves it; `E` is an invalid transition.

## Transition matrix

| State | L | i | s | D | _ | . | $ | +- | / | op*() | = | <> | !: | " | nl | ws | other |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 1 | 1 | 1 | 5 | E | 7 | E | F_CONS | 2 | F_CONS | 13 | 14 | 4 | 12 | 0 | 0 | E |
| 1 | 1 | 1 | 1 | 1 | 1 | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET |
| 2 | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | 3 | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET |
| 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 0 | 3 | 3 |
| 4 | E | E | E | E | E | E | E | E | E | E | F_CONS | E | E | E | E | E | E |
| 5 | E | E | E | 5 | E | 7 | 6 | E | E | E | E | E | E | E | E | E | E |
| 6 | E | F_CONS | E | E | E | E | E | E | E | E | E | E | E | E | E | E | E |
| 7 | F_RET | F_RET | F_RET | 8 | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET |
| 8 | F_RET | F_RET | 9 | 8 | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET |
| 9 | E | E | E | 11 | E | E | E | 10 | E | E | E | E | E | E | E | E | E |
| 10 | E | E | E | 11 | E | E | E | E | E | E | E | E | E | E | E | E | E |
| 11 | F_RET | F_RET | F_RET | 11 | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET |
| 12 | 12 | 12 | 12 | 12 | 12 | 12 | 12 | 12 | 12 | 12 | 12 | 12 | 12 | F_CONS | 12 | 12 | 12 |
| 13 | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_CONS | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET |
| 14 | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET | F_CONS | F_RET | F_RET | F_RET | F_RET | F_RET | F_RET |

## Semantic-action matrix

| State | L | i | s | D | _ | . | $ | +- | / | op*() | = | <> | !: | " | nl | ws | other |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | sa_init | sa_init | sa_init | sa_init | sa_error | sa_init | sa_error | sa_token_consume | sa_init | sa_token_consume | sa_init | sa_init | sa_init | sa_init_chain | sa_ignore | sa_ignore | sa_error |
| 1 | sa_append | sa_append | sa_append | sa_append | sa_append | sa_identifier | sa_identifier | sa_identifier | sa_identifier | sa_identifier | sa_identifier | sa_identifier | sa_identifier | sa_identifier | sa_identifier | sa_identifier | sa_identifier |
| 2 | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_ignore | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered |
| 3 | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore | sa_ignore |
| 4 | sa_error | sa_error | sa_error | sa_error | sa_error | sa_error | sa_error | sa_error | sa_error | sa_error | sa_multi_char_op | sa_error | sa_error | sa_error | sa_error | sa_error | sa_error |
| 5 | sa_integer_literal_error | sa_integer_literal_error | sa_float_decimal_error | sa_append | sa_integer_literal_error | sa_append | sa_append | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error |
| 6 | sa_integer_literal_error | sa_int_const | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error | sa_integer_literal_error |
| 7 | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_append | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error | sa_dot_or_float_error |
| 8 | sa_float_const | sa_float_const | sa_append | sa_append | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const |
| 9 | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_append | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_append | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error |
| 10 | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_append | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error | sa_float_exponent_error |
| 11 | sa_float_const | sa_float_const | sa_float_const | sa_append | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const | sa_float_const |
| 12 | sa_append | sa_append | sa_append | sa_append | sa_append | sa_append | sa_append | sa_append | sa_append | sa_append | sa_append | sa_append | sa_append | sa_chain | sa_append | sa_append | sa_append |
| 13 | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_multi_char_op | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered |
| 14 | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_multi_char_op | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered | sa_token_buffered |
