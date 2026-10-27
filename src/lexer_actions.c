#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "../include/diagnostics.h"
#include "../include/lexer_actions.h"
#include "../include/symbol_table.h"
#include "../y.tab.h"

char *lexeme_buffer = NULL;
size_t lexeme_capacity = 0;
int lexeme_length = 0;

static void ensure_lexeme_capacity(size_t needed) {
    if (lexeme_buffer == NULL) {
        lexeme_capacity = (needed > 128) ? needed + 64 : 128;
        lexeme_buffer = (char *)malloc(lexeme_capacity);
        if (lexeme_buffer == NULL) {
            fprintf(stderr, "Fatal error: Cannot allocate lexeme buffer\n");
            exit(1);
        }
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
extern YYSTYPE yylval;



sem_act_t sem_act_mat[N_STATES][N_COLS] = {
    //              L               i                      s                      D                 _                        .                 $                 +-                /                 op*()             =                 <>                !:                "                 nl                ws                other           EOF
    {  sa_init,        sa_init,          sa_init,          sa_init,          sa_error,         sa_init,          sa_error,         sa_token_consume, sa_init,          sa_token_consume, sa_init,          sa_init,          sa_init,          sa_init_chain,           sa_ignore,        sa_ignore,        sa_error, sa_EOF }, // 0
    {  sa_append,      sa_append,        sa_append,        sa_append,        sa_append,        sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier, sa_identifier }, // 1
    {  sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_ignore,        sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered }, // 2
    {  sa_ignore,      sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore, sa_ignore }, // 3
    {  sa_operator_error,       sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_multi_char_op, sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_operator_error,         sa_operator_error, sa_operator_error }, // 4
    {  sa_integer_literal_error, sa_integer_literal_error, sa_float_decimal_error, sa_append, sa_integer_literal_error, sa_append, sa_append, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error }, // 5
    {  sa_integer_literal_error, sa_int_const, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error, sa_integer_literal_error }, // 6
    {  sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_append, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error, sa_dot_or_float_error }, // 7
    {  sa_float_const, sa_float_const,   sa_append,        sa_append,        sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const, sa_float_const }, // 8
    {  sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_append, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_append, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error }, // 9
    {  sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_append, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error, sa_float_exponent_error }, // 10
    {  sa_float_const, sa_float_const,   sa_float_const,   sa_append,        sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const, sa_float_const }, // 11
    {  sa_append,      sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_chain,         sa_append,        sa_append,        sa_append, sa_unclosed_chain_error }, // 12
    {  sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_multi_char_op, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered }, // 13
    {  sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_multi_char_op, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered, sa_token_buffered }  // 14
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

int sa_unclosed_chain_error(char c) {
    (void)c;
    report_lexical_error(token_start_line, "Unclosed string literal");
    return YYerror;
}

int sa_EOF(char c) {
    (void)c;
    return YYEOF;
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

int sa_token_consume(char c) {
    lexeme_length = 0;
    append_to_lexeme(c);
    return (unsigned char)c;
}

int sa_token_buffered(char c) {
    (void)c;
    if (lexeme_buffer == NULL || lexeme_length == 0) {
        return -1;
    }
    return (unsigned char)lexeme_buffer[0];
}

int sa_ignore(char c) { (void)c;
    return -1;
}

int sa_identifier(char c) {
    (void)c;
    char *lower = strdup(lexeme_buffer);
    if (lower == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate identifier text\n");
        exit(1);
    }
    for (int i = 0; i < lexeme_length; i++) {
        lower[i] = tolower((unsigned char)lexeme_buffer[i]);
    }
    int token = check_reserved_words(lower);
    free(lower);
    if (token != -1) return token;

    for (int i = 0; i < lexeme_length; i++) {
        if (isupper((unsigned char)lexeme_buffer[i])) {
            report_lexical_error(token_start_line,
                                 "Identifier '%s' contains uppercase letters", lexeme_buffer);
            return YYerror;
        }
    }
    if (lexeme_length > 22) {
        printf("Line %d: Warning: Identifier '%s' was truncated to: '%.*s'.\n",
               token_start_line, lexeme_buffer, 22, lexeme_buffer);
        lexeme_length = 22;
        lexeme_buffer[22] = '\0';
    }
    yylval.symbol_ref = add_to_symbol_table(lexeme_buffer, "ID");
    return TOKEN_ID;
}

int sa_int_const(char c) {
    append_to_lexeme(c);
    errno = 0;
    char *end;
    long value = strtol(lexeme_buffer, &end, 10);
    if (errno == ERANGE || value > 32768) {
        report_lexical_error(token_start_line,
                             "Integer constant '%s' out of range", lexeme_buffer);
        return YYerror;
    }
    if (strcmp(end, "$i") != 0) {
        return sa_integer_literal_error(c);
    }
    yylval.symbol_ref = add_to_symbol_table(lexeme_buffer, "INTEGER");
    /* TP2 resolves the sign in constant; positive 32768 is not a final symbol. */
    yylval.symbol_ref->provisional = value == 32768;
    return TOKEN_CONST;
}

int sa_float_const(char c) {
    (void)c;
    char *numeric_text = strdup(lexeme_buffer);
    if (numeric_text == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate floating constant text\n");
        exit(1);
    }
    for (int i = 0; numeric_text[i]; i++) {
        if (numeric_text[i] == 's') numeric_text[i] = 'e';
    }
    errno = 0;
    char *end;
    double value = strtod(numeric_text, &end);
    double magnitude = value < 0 ? -value : value;
    bool invalid = errno == ERANGE || *end != '\0' ||
        (magnitude > 0 && (magnitude <= 1.17549435e-38 || magnitude >= 3.40282347e+38));
    free(numeric_text);
    if (invalid) {
        report_lexical_error(token_start_line,
                             "Float constant '%s' out of range", lexeme_buffer);
        return YYerror;
    }
    yylval.symbol_ref = add_to_symbol_table(lexeme_buffer, "SINGLEF");
    return TOKEN_CONST;
}

int sa_init_chain(char c) { (void)c;
    reset_lexeme_buffer();
    return -1;
}

int sa_chain(char c) { (void)c;
    yylval.symbol_ref = add_to_symbol_table(lexeme_buffer, "CHAIN");
    return TOKEN_CHAIN;
}

int sa_multi_char_op(char c) {
    append_to_lexeme(c);
    if (strcmp(lexeme_buffer, ":=") == 0) return TOKEN_ASSIGN;
    if (strcmp(lexeme_buffer, ">=") == 0) return TOKEN_GREATER_EQUAL;
    if (strcmp(lexeme_buffer, "<=") == 0) return TOKEN_LESS_EQUAL;
    if (strcmp(lexeme_buffer, "==") == 0) return TOKEN_EQUAL;
    if (strcmp(lexeme_buffer, "!=") == 0) return TOKEN_NOT_EQUAL;
    report_lexical_error(token_start_line, "Invalid operator '%s'", lexeme_buffer);
    return YYerror;
}

int sa_dot_or_float_error(char c) {
    (void)c;
    if (lexeme_length == 1 && lexeme_buffer[0] == '.') return '.';
    report_lexical_error(token_start_line, "Float constant requires a decimal part");
    return YYerror;
}

int sa_float_decimal_error(char c) {
    (void)c;
    report_lexical_error(token_start_line,
                         "Float constant requires a decimal part before its exponent");
    return YYerror;
}

int sa_integer_literal_error(char c) {
    (void)c;
    if (strchr(lexeme_buffer, '$') != NULL) {
        report_lexical_error(token_start_line,
            "Malformed integer constant '%s'; expected suffix '$i'", lexeme_buffer);
    } else {
        report_lexical_error(token_start_line,
            "Integer constant '%s' is missing required suffix '$i'", lexeme_buffer);
    }
    return YYerror;
}

int sa_float_exponent_error(char c) {
    (void)c;
    report_lexical_error(token_start_line,
        "Malformed SINGLEF constant '%s'; exponent marker 's' must be followed by digits",
        lexeme_buffer);
    return YYerror;
}

static const char *display_character(int c) {
    static char value[2];
    switch (c) {
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case ' ': return "space";
        default:
            value[0] = (char)c;
            value[1] = '\0';
            return value;
    }
}

int sa_error(char c) {
    report_lexical_error(token_start_line, "Invalid character '%s'",
                         display_character((unsigned char)c));
    return YYerror;
}

int sa_operator_error(char c) {
    (void)c;
    report_lexical_error(token_start_line,
                         "Incomplete operator '%s'; expected '='", lexeme_buffer);
    return YYerror;
}
