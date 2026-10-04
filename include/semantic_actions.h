#ifndef SEMANTIC_ACTIONS_H 
#define SEMANTIC_ACTIONS_H

#include <stdio.h>
#include "../y.tab.h"
#include "lexer.h"

extern char *lexeme_buffer;
extern int lexeme_length;
void reset_lexeme_buffer(void);
void free_lexeme_buffer(void);
extern FILE *source_file;
extern int current_line;
extern YYSTYPE yylval;
extern int token_start_line;
extern int global_errors;
extern int lex_range_reported;
typedef int (*sem_act_t)(char);

extern sem_act_t sem_act_mat[15][17];

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
int sa_error(char c);

int check_reserved_words(const char* word);
void add_to_symbol_table(const char* lex, const char* type);
extern void print_symbol_table();

#endif /* SEMANTIC_ACTIONS_H */
