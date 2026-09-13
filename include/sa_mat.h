#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

#define TOKEN_IF 256
#define TOKEN_ELSE 257
#define TOKEN_END_IF 258
#define TOKEN_BEGIN 259
#define TOKEN_END 260
#define TOKEN_POUT 261
#define TOKEN_RET 262
#define TOKEN_CLASS 263
#define TOKEN_FUNCTION 264
#define TOKEN_INTEGER 265
#define TOKEN_SINGLEF 266

#define TOKEN_ASSIGN 267
#define TOKEN_GREATER_EQUAL 268
#define TOKEN_LESS_EQUAL 269
#define TOKEN_EQUAL 270
#define TOKEN_NOT_EQUAL 271

#define TOKEN_CONST 272
#define TOKEN_STRING 273
#define TOKEN_FROM 274
#define TOKEN_TO 275
#define TOKEN_BY 276
#define TOKEN_REPEAT 277
#define TOKEN_COMPTIME 278
#define TOKEN_TOI 279
#define TOKEN_POUT_LOWER 280
#define TOKEN_ID 281

extern char lexema[100];
extern int lex_idx;
extern FILE *s_file;
extern int state;

typedef int (*sem_act)(char);

int yylex(void);

int sa_init(char c);
int sa_append(char c);
int sa_ascii_token(char c);
int sa_ignore(char c);
int sa_identifier(char c);
int sa_int_const(char c);
int sa_float_const(char c);
int sa_string(char c);
int sa_multi_char_op(char c);
int sa_error(char c);

int check_reserved_words(const char* word);
void add_to_symbol_table(const char* lex, const char* type);

#endif