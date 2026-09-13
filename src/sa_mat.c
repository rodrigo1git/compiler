#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <math.h>

char lexema[100];
int lex_idx;
FILE *s_file;
int state;

typedef int (*sem_act_t)(char);

sem_act sem_act_mat_t[18][21] = {
    {sa_init, sa_init, sa_error, sa_error, sa_init, sa_init, sa_init, sa_error, sa_init, sa_ascii_token, sa_ascii_token, sa_ascii_token, sa_ignore, sa_ignore, sa_error, sa_error, sa_init, sa_error, sa_error, sa_error, sa_error},
    {sa_append, sa_append, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_identifier},
    {sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_ignore, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_ascii_token},
    {sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_ignore, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_ignore},
    {sa_error, sa_error, sa_error, sa_error, sa_error, sa_multi_char_op, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_ascii_token},
    {sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_append, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error},
    {sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_int_const, sa_error, sa_error, sa_error, sa_error, sa_error},
    {sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error},
    {sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_float_const},
    {sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error},
    {sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error},
    {sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_float_const},
    {sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_string, sa_error, sa_error, sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error},
    {sa_error, sa_error, sa_error, sa_error, sa_multi_char_op, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error},
    {sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_ascii_token, sa_ascii_token, sa_error},
    {sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_append, sa_error, sa_error, sa_error, sa_error, sa_error},
    {sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_ascii_token, sa_error, sa_error},
    {sa_error, sa_init, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error, sa_error}
};

int check_reserved_words(const char* word) {
    if (strcmp(word, "if") == 0) return TOKEN_IF;
    if (strcmp(word, "else") == 0) return TOKEN_ELSE;
    if (strcmp(word, "end_if") == 0) return TOKEN_END_IF;
    if (strcmp(word, "begin") == 0) return TOKEN_BEGIN;
    if (strcmp(word, "end") == 0) return TOKEN_END;
    if (strcmp(word, "ret") == 0) return TOKEN_RET;
    if (strcmp(word, "class") == 0) return TOKEN_CLASS;
    if (strcmp(word, "function") == 0) return TOKEN_FUNCTION;
    if (strcmp(word, "integer") == 0) return TOKEN_INTEGER;
    if (strcmp(word, "singlef") == 0) return TOKEN_SINGLEF;
    if (strcmp(word, "from") == 0) return TOKEN_FROM;
    if (strcmp(word, "to") == 0) return TOKEN_TO;
    if (strcmp(word, "by") == 0) return TOKEN_BY;
    if (strcmp(word, "repeat") == 0) return TOKEN_REPEAT;
    if (strcmp(word, "comptime") == 0) return TOKEN_COMPTIME;
    if (strcmp(word, "toi") == 0) return TOKEN_TOI;
    if (strcmp(word, "pout") == 0) return TOKEN_POUT_LOWER;
    
    return -1;
}

int sa_init(char c) {
    lex_idx = 0;
    lexema[lex_idx++] = c;
    lexema[lex_idx] = '\0';
    return -1;
}

int sa_append(char c) {
    if (c != '\n' && c != '\r') {
        lexema[lex_idx++] = c;
        lexema[lex_idx] = '\0';
    }
    return -1;
}

int sa_ascii_token(char c) {
    switch (state) {
        case 0:
        case 14:
        case 16:
            return c;
            
        case 2:
            if (c != EOF) {
                ungetc(c, s_file);
            }
            return '<';
            
        case 4:
            if (c != EOF) {
                ungetc(c, s_file);
            }
            return '>';
            
        default:
            return -1;
    }
}

int sa_ignore(char c) {
    return -1;
}

int sa_identifier(char c) {
    if (c != EOF) {
        ungetc(c, s_file);
    }
    
    if (lex_idx > 22) {
        printf("Warning: Identifier exceeds 22 characters. Truncating.\n");
        lex_idx = 22;
        lexema[lex_idx] = '\0';
    }
    
    for (int i = 0; i < lex_idx; i++) {
        lexema[i] = tolower(lexema[i]);
    }
    
    int token = check_reserved_words(lexema);
    if (token == -1) {
        add_to_symbol_table(lexema, "ID");
        token = TOKEN_ID;
    }
    
    return token;
}

int sa_int_const(char c) {
    if (c != EOF) {
        ungetc(c, s_file);
    }
    
    long value = atol(lexema);
    if (value < -32768 || value > 32767) {
        printf("Error: Integer constant %ld out of 16-bit range.\n", value);
    }
    
    add_to_symbol_table(lexema, "INTEGER");
    return TOKEN_CONST;
}

int sa_float_const(char c) {
    if (c != EOF) {
        ungetc(c, s_file);
    }
    
    double value = atof(lexema);
    if (fabs(value) > FLT_MAX || (fabs(value) > 0.0 && fabs(value) < FLT_MIN)) {
        printf("Error: Float constant out of 32-bit range.\n");
    }
    
    add_to_symbol_table(lexema, "FLOAT");
    return TOKEN_CONST;
}

int sa_string(char c) {
    add_to_symbol_table(lexema, "STRING");
    return TOKEN_STRING;
}

int sa_multi_char_op(char c) {
    if (strcmp(lexema, ":=") == 0) return TOKEN_ASSIGN;
    if (strcmp(lexema, ">=") == 0) return TOKEN_GREATER_EQUAL;
    if (strcmp(lexema, "<=") == 0) return TOKEN_LESS_EQUAL;
    if (strcmp(lexema, "==") == 0) return TOKEN_EQUAL;
    if (strcmp(lexema, "!=") == 0) return TOKEN_NOT_EQUAL;
    
    return -1;
}

int sa_error(char c) {
    return -1;
}
