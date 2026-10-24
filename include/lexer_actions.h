#ifndef LEXER_ACTIONS_H
#define LEXER_ACTIONS_H

#include <stdio.h>
#include "../y.tab.h"
#include "lexer.h"
#include "transition_table.h"

extern char *lexeme_buffer;
extern int lexeme_length;
void reset_lexeme_buffer(void);
void free_lexeme_buffer(void);
extern FILE *source_file;
extern YYSTYPE yylval;
extern int token_start_line;
typedef int (*sem_act_t)(char);

extern sem_act_t sem_act_mat[N_STATES][N_COLS];

int sa_init(char c);
int sa_append(char c);
int sa_token_consume(char c);
int sa_token_buffered(char c);
int sa_ignore(char c);
int sa_identifier(char c);
int sa_int_const(char c);
int sa_float_const(char c);
int sa_init_chain(char c);
int sa_chain(char c);
int sa_multi_char_op(char c);
int sa_dot_or_float_error(char c);
int sa_integer_literal_error(char c);
int sa_float_exponent_error(char c);
int sa_float_decimal_error(char c);
int sa_error(char c);
int sa_operator_error(char c);

int check_reserved_words(const char* word);

#endif /* LEXER_ACTIONS_H */
