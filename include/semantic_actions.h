#ifndef SA_MAT_H 
#define LEXER_H

#include <stdio.h>
#include "../y.tab.h"
#include "lexer.h"

extern char lexeme_buffer[100];
extern int lexeme_length;
extern FILE *source_file;
extern int state;
extern int current_line;
extern YYSTYPE yylval;
typedef int (*sem_act_t)(char);

extern sem_act_t sem_act_mat[15][17];

int sa_init(char c);
int sa_append(char c);
int sa_ascii_token(char c);
int sa_ignore(char c);
int sa_identifier(char c);
int sa_int_const(char c);
int sa_float_const(char c);
int sa_init_string(char c);
int sa_string(char c);
int sa_multi_char_op(char c);
int sa_error(char c);

int check_reserved_words(const char* word);
void add_to_symbol_table(const char* lex, const char* type);
extern void print_symbol_table();

#endif /* SEMANTIC_ACTIONS_H */
