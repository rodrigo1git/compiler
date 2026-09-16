//#include <lexer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include "../include/sa_mat.h"
#include "../include/map.h"

char lexema[100];
int lex_idx;
extern FILE *s_file;
extern int state;
extern int cant_lin;

typedef int (*sem_act_t)(char);

sem_act_t sem_act_mat[15][17] = {
    // L               i                 s                 D                 _                 .                 $                 +-                /                 op*()             =                 <>                !:                "                 nl                ws                otro
    {  sa_init,        sa_init,          sa_init,          sa_init,          sa_error,         sa_init,          sa_error,         sa_ascii_token,   sa_init,          sa_ascii_token,   sa_init,          sa_init,          sa_init,          sa_init_string,          sa_ignore,        sa_ignore,        sa_error        }, // 0
    {  sa_append,      sa_append,        sa_append,        sa_append,        sa_append,        sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier,    sa_identifier   }, // 1
    {  sa_ascii_token, sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ignore,        sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token  }, // 2
    {  sa_ignore,      sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore,        sa_ignore       }, // 3
    {  sa_error,       sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_multi_char_op, sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 4
    {  sa_error,       sa_error,         sa_error,         sa_append,        sa_error,         sa_append,        sa_append,        sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 5
    {  sa_error,       sa_int_const,     sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 6
    {  sa_error,       sa_error,         sa_error,         sa_append,        sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 7
    {  sa_float_const, sa_float_const,   sa_append,        sa_append,        sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const  }, // 8
    {  sa_error,       sa_error,         sa_error,         sa_append,        sa_error,         sa_error,         sa_error,         sa_append,        sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 9
    {  sa_error,       sa_error,         sa_error,         sa_append,        sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error,         sa_error        }, // 10
    {  sa_float_const, sa_float_const,   sa_float_const,   sa_append,        sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const,   sa_float_const  }, // 11
    {  sa_append,      sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_append,        sa_string,        sa_append,        sa_append,        sa_append       }, // 12
    {  sa_ascii_token, sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_multi_char_op, sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token  }, // 13
    {  sa_ascii_token, sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_multi_char_op, sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token,   sa_ascii_token  }  // 14
};
const char* sem_act_names[15][17] = {
    // L                   i                   s                   D                   _                   .                   $                   +-                  /                   op*()               =                   <>                  !:                  "                   nl                  ws                  otro
    {"sa_init",          "sa_init",          "sa_init",          "sa_init",          "sa_error",         "sa_init",          "sa_error",         "sa_ascii_token",   "sa_init",          "sa_ascii_token",   "sa_init",          "sa_init",          "sa_init",          "sa_init",          "sa_ignore",        "sa_ignore",        "sa_error"        }, // 0
    {"sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier",    "sa_identifier"   }, // 1
    {"sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ignore",        "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token"  }, // 2
    {"sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore",        "sa_ignore"       }, // 3
    {"sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_multi_char_op", "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 4
    {"sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_append",        "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 5
    {"sa_error",         "sa_int_const",     "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 6
    {"sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 7
    {"sa_float_const",   "sa_float_const",   "sa_append",        "sa_append",        "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const"  }, // 8
    {"sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 9
    {"sa_error",         "sa_error",         "sa_error",         "sa_append",        "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error",         "sa_error"        }, // 10
    {"sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_append",        "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const",   "sa_float_const"  }, // 11
    {"sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_append",        "sa_string",        "sa_append",        "sa_append",        "sa_append"       }, // 12
    {"sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_multi_char_op", "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token"  }, // 13
    {"sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_multi_char_op", "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token",   "sa_ascii_token"  }  // 14
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

void add_to_symbol_table(const char* lexema, const char* tipo) {
    if (!map_contains(symbol_table, lexema)) {
        map_put(symbol_table, lexema, tipo);
    }
}

int sa_ascii_token(char c) {
    switch (state) {
        case 0:
            // Símbolos directos desde el estado 0
            lex_idx = 0;
            lexema[lex_idx++] = c;
            lexema[lex_idx] = '\0';
            return (unsigned char)c;
            
        case 2:
            // Barra de división aislada '/'
            if (c != EOF) {
                if (c == '\n') cant_lin--;
                ungetc(c, s_file);
            }
            return '/';
            
        case 13:
            // Operador de asignación aislado '='
            if (c != EOF) {
                if (c == '\n') cant_lin--;
                ungetc(c, s_file);
            }
            return '=';
            
        case 14:
            // Operadores relacionales '<' o '>'
            if (c != EOF) {
                if (c == '\n') cant_lin--;
                ungetc(c, s_file);
            }
            return lexema[0];
            
        default:
            return -1;
    }
}

int sa_ignore(char c) {
    return -1;
}

int sa_identifier(char c) {
    if (c != EOF) {
        if (c == '\n') cant_lin--;
        ungetc(c, s_file);
    }
    
    char lexema_lower[256];
    for (int i = 0; i < lex_idx; i++) {
        lexema_lower[i] = tolower(lexema[i]);
    }
    lexema_lower[lex_idx] = '\0';
    
    int token = check_reserved_words(lexema_lower);
    
    if (token != -1) {
        return token;
    }
    
    for (int i = 0; i < lex_idx; i++) {
        if (isupper(lexema[i])) {
            printf("Error léxico: El identificador contiene mayúsculas.\n");
            return -1; 
        }
    }
    
    if (lex_idx > 22) {
        printf("Warning: El identificador supera los 22 caracteres. Truncando.\n");
        lex_idx = 22;
        lexema[lex_idx] = '\0';
    }
    add_to_symbol_table(lexema, "ID");
    return TOKEN_ID;
}

int sa_int_const(char c) {
    lexema[lex_idx++] = c;
    lexema[lex_idx] = '\0';
    if (c != EOF) {
        if (c == '\n') cant_lin--;
        // ungetc(c, s_file);
    }
    
    long val = atol(lexema);
    if (val < -32768 || val > 32767) {
        return -1; 
    }
    add_to_symbol_table(lexema, "INTEGER");
    return TOKEN_CONST;
}

int sa_float_const(char c) {
    if (c != EOF){  
        ungetc(c, s_file);
        if (c == '\n') cant_lin--;
    }
    
    for (int i = 0; lexema[i] != '\0'; i++) {
        if (lexema[i] == 's') lexema[i] = 'e';
    }
    
    double val = atof(lexema);
    double abs_val = val < 0 ? -val : val;
    
    if (abs_val > 0.0 && (abs_val < 1.17549435e-38 || abs_val > 3.40282347e+38)) {
        return -1;
    }
    
    add_to_symbol_table(lexema, "FLOAT");

    return TOKEN_CONST;
}

int sa_init_string(char c) {
      lex_idx = 0;
      lexema[0] = '\0'; // Reinicia el buffer pero ignora la 'c' (la comilla de apertura)
      return -1;
}
int sa_string(char c) {
    add_to_symbol_table(lexema, "STRING");
    return TOKEN_STRING;
}

int sa_multi_char_op(char c) {
    lexema[lex_idx++] = c;
    lexema[lex_idx] = '\0';
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
void print_symbol_table() {
    if (symbol_table != NULL) {
        map_print(symbol_table);
    }
}
