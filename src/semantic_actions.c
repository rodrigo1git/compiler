#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include "../include/semantic_actions.h"
#include "../include/symbol_table.h"
#include "../y.tab.h"

char *lexeme_buffer = NULL;
size_t lexeme_capacity = 0;
int lexeme_length = 0;

static void ensure_lexeme_capacity(size_t needed) {
    if (lexeme_buffer == NULL) {
        lexeme_capacity = (needed > 128) ? needed + 64 : 128;
        lexeme_buffer = (char *)malloc(lexeme_capacity);
        lexeme_buffer[0] = '\0';
    } else if (needed >= lexeme_capacity) {
        while (needed >= lexeme_capacity) {
            lexeme_capacity *= 2;
        }
        char *tmp = (char *)realloc(lexeme_buffer, lexeme_capacity);
        if (tmp == NULL) {
            fprintf(stderr, "Fatal Error: Memory allocation failed during realloc.\n");
            exit(1);
        }
        lexeme_buffer = tmp;
    }
}

static void append_to_lexeme(char c) {
    ensure_lexeme_capacity(lexeme_length + 2);
    lexeme_buffer[lexeme_length++] = c;
    lexeme_buffer[lexeme_length] = '\0';
}

void reset_lexeme_buffer(void) {
    ensure_lexeme_capacity(1);
    lexeme_length = 0;
    lexeme_buffer[0] = '\0';
}

void free_lexeme_buffer(void) {
    if (lexeme_buffer != NULL) {
        free(lexeme_buffer);
        lexeme_buffer = NULL;
        lexeme_capacity = 0;
        lexeme_length = 0;
    }
}

extern FILE *source_file;
extern int current_line;
extern YYSTYPE yylval;

typedef int (*sem_act_t)(char);

sem_act_t sem_act_mat[15][17] = {
    //              L               i                      s                      D                 _                        .                 $                 +-                /                 op*()             =                 <>                !:                "                 nl                ws                other
    {  sa_init,        sa_init,          sa_init,          sa_init,          sa_error,         sa_init,          sa_error,         sa_token_consume, sa_init,          sa_token_consume, sa_init,          sa_init,          sa_init,          sa_init_chain,           sa_ignore,        sa_ignore,        sa_error        }, // 0
    {  sa_append,      sa_append,        sa_append,        sa_append,        sa_append,        sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier   }, // 1
    {  sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_ignore,        sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered }, // 2
    {  sa_ignore,      sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore       }, // 3
    {  sa_error,       sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_multi_char_op, sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 4
    {  sa_error,       sa_error,         sa_error,         sa_append,        sa_error,         sa_append,        sa_append,        sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 5
    {  sa_error,       sa_int_const,     sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 6
    {  sa_error,       sa_error,         sa_error,         sa_append,        sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 7
    {  sa_float_const, sa_float_const,   sa_append,        sa_append,        sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const  }, // 8
    {  sa_error,       sa_error,         sa_error,         sa_append,        sa_error,         sa_error,         sa_error,         sa_append,        sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 9
    {  sa_error,       sa_error,         sa_error,         sa_append,        sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 10
    {  sa_float_const, sa_float_const,   sa_float_const,   sa_append,        sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const  }, // 11
    {  sa_append,      sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_chain,         sa_append,        sa_append,        sa_append       }, // 12
    {  sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_multi_char_op, sa_multi_char_op, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered }, // 13
    {  sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_multi_char_op, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered }  // 14
};
const char* sem_act_names[15][17] = {
    // L                   i                   s                   D                   _                   .                   $                   +-                  /                   op*()               =                   <>                  !:                  "                   nl                  ws                  other
    {"sa_init",          "sa_init",          "sa_init",          "sa_init",          "sa_error",         "sa_init",          "sa_error",         "sa_token_consume", "sa_init",          "sa_token_consume", "sa_init",          "sa_init",          "sa_init",          "sa_init_chain",    "sa_ignore",        "sa_ignore",        "sa_error"        }, // 0
    {"sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier"   }, // 1
    {"sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_ignore",        "sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered" }, // 2
    {"sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore"       }, // 3
    {"sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_multi_char_op", "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 4
    {"sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_append",        "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 5
    {"sa_error",         "sa_int_const",     "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 6
    {"sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 7
    {"sa_float_const",   "sa_float_const",   "sa_append",        "sa_append",        "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const"  }, // 8
    {"sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 9
    {"sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 10
    {"sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_append",        "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const"  }, // 11
    {"sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_chain",         "sa_append",        "sa_append",        "sa_append"       }, // 12
    {"sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_multi_char_op", "sa_multi_char_op", "sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered"}, // 13
    {"sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_multi_char_op", "sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered","sa_token_buffered"}  // 14
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
    if (strcmp(word, "extends") == 0) return TOKEN_EXTENDS;
    
    return -1;
}

int sa_init(char c) {
    lexeme_length = 0;
    append_to_lexeme(c);
    return -1;
}

int sa_append(char c) {
    if (c != '\n' && c != '\r') {
        append_to_lexeme(c);
    }
    return -1;
}

void add_to_symbol_table(const char* lexeme_buffer, const char* tipo) {
    map_put(symbol_table, lexeme_buffer, tipo);
}

int sa_token_consume(char c) {
    lexeme_length = 0;
    append_to_lexeme(c);
    return (unsigned char)c;
}

int sa_token_buffered(char c) {
    (void)c;
    return (lexeme_buffer != NULL) ? (unsigned char)lexeme_buffer[0] : (unsigned char)c;
}

int sa_ignore(char c) { (void)c;
    return -1;
}

int sa_identifier(char c) { (void)c;
    char *original_lexeme = strdup(lexeme_buffer ? lexeme_buffer : "");
    char *lexeme_buffer_lower = (char *)malloc(lexeme_length + 1);
    for (int i = 0; i < lexeme_length; i++) {
        lexeme_buffer_lower[i] = tolower((unsigned char)lexeme_buffer[i]);
    }
    lexeme_buffer_lower[lexeme_length] = '\0';
    
    int token = check_reserved_words(lexeme_buffer_lower);
    if (token != -1) {
        free(original_lexeme);
        free(lexeme_buffer_lower);
        return token;
    }
    
    for (int i = 0; i < lexeme_length; i++) {
        if (isupper((unsigned char)lexeme_buffer[i])) {
            printf("Lexical error: Identifier contains uppercase letters.\n");
            free(original_lexeme);
            free(lexeme_buffer_lower);
            return -1; 
        }
    }
    
    if (lexeme_length > 22) {
        lexeme_buffer_lower[22] = '\0';
        lexeme_length = 22;
        lexeme_buffer[lexeme_length] = '\0';
        printf("Line %d: Warning: Identifier '%s' was truncated to: '%s'.\n", current_line, original_lexeme, lexeme_buffer_lower);
    }
    add_to_symbol_table(lexeme_buffer, "ID");
    free(original_lexeme);
    free(lexeme_buffer_lower);
    yylval.str_val = strdup(lexeme_buffer);
    return TOKEN_ID;
}

int sa_int_const(char c) {
    append_to_lexeme(c);
    long val = atol(lexeme_buffer);
    
    if (val > 32768) {
        fprintf(stderr, "Line %d: Lexical error: Integer constant '%s' out of range\n", current_line, lexeme_buffer);
        return -1;
    }
    
    add_to_symbol_table(lexeme_buffer, "INTEGER");
    yylval.str_val = strdup(lexeme_buffer); 
    return TOKEN_CONST;
}

int sa_float_const(char c) { (void)c;
    for (int i = 0; lexeme_buffer[i] != '\0'; i++) {
        if (lexeme_buffer[i] == 's') lexeme_buffer[i] = 'e';
    }
    
    double val = atof(lexeme_buffer);
    double abs_val = val < 0 ? -val : val;
    
    if (abs_val > 0.0 && (abs_val < 1.17549435e-38 || abs_val > 3.40282347e+38)) {
        fprintf(stderr, "Line %d: Lexical error: Float constant '%s' out of range\n", current_line, lexeme_buffer);
        return -1;
    }
    
    add_to_symbol_table(lexeme_buffer, "SINGLEF");
    yylval.str_val = strdup(lexeme_buffer);
    return TOKEN_CONST;
}

int sa_init_chain(char c) { (void)c;
    reset_lexeme_buffer();
    return -1;
}

int sa_chain(char c) { (void)c;
    add_to_symbol_table(lexeme_buffer, "CHAIN");
    yylval.str_val = strdup(lexeme_buffer);
    return TOKEN_CHAIN;
}

int sa_multi_char_op(char c) {
    append_to_lexeme(c);
    if (strcmp(lexeme_buffer, ":=") == 0) return TOKEN_ASSIGN;
    if (strcmp(lexeme_buffer, ">=") == 0) return TOKEN_GREATER_EQUAL;
    if (strcmp(lexeme_buffer, "=>") == 0) return TOKEN_GREATER_EQUAL;
    if (strcmp(lexeme_buffer, "<=") == 0) return TOKEN_LESS_EQUAL;
    if (strcmp(lexeme_buffer, "=<") == 0) return TOKEN_LESS_EQUAL;
    if (strcmp(lexeme_buffer, "==") == 0) return TOKEN_EQUAL;
    if (strcmp(lexeme_buffer, "!=") == 0) return TOKEN_NOT_EQUAL;
    return -1;
}

int sa_error(char c) { (void)c;
    return -1;
}
void print_symbol_table() {
    if (symbol_table != NULL) {
        map_print(symbol_table);
    }
}
