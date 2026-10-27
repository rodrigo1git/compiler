#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include "../include/diagnostics.h"

#include "../include/lexer.h"
#include "../include/lexer_actions.h"
#include "../include/transition_table.h"

int current_line = 1;

static int previous_char;

static int update_line_number(int c) {
    int advanced = 0;
    if (c == '\n' && previous_char != '\r') {
        current_line++;
        advanced = 1;
    } else if (c == '\r') {
        current_line++;
        advanced = 1;
    }
    previous_char = c;
    return advanced;
}

static bool is_recovery_delimiter(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == ';' || c == ')' || c == ']' || c == ',';
}

static bool is_valid_token_boundary(int c) {
    if (c == EOF) {
        return false;
    }
    if (get_col(c) == COL_OTHER) {
        return false;
    }
    if (isalnum((unsigned char)c) || c == '_' || c == '$') {
        return false;
    }
    return true;
}

static void push_back_char(int c, int line_advanced) {
    if (line_advanced) {
        current_line--;
    }

    if (c != EOF) {
        (void)ungetc(c, source_file);
        previous_char = '\0';
    }
}

static void recover_from_lexical_error(int c, int line_advanced) {
    while (!is_recovery_delimiter(c) && c != EOF) {
        c = getc(source_file);
        line_advanced = update_line_number(c);
    }

    if (c != EOF) {
        push_back_char(c, line_advanced);
    }

    reset_lexeme_buffer();
}

static void set_token_location(void) {
    yylloc.first_line = token_start_line;
    yylloc.last_line = current_line;
    yylloc.first_column = 1;
    yylloc.last_column = 1;
}

static void log_token(int token_id) {
    trace_printf("[LEX] Token: %d | Lexeme: \"%s\" | Line: %d\n",
           token_id, lexeme_buffer, token_start_line);
}

void lexer_reset_input(void) {
    previous_char = '\0';
    current_line = 1;
    token_start_line = 1;
    reset_lexeme_buffer();
}

static int state = 0;

int yylex(void) {
    

    while(true) {
        if (state == 0) {
            token_start_line = current_line;
        }

        int c = getc(source_file);

        int line_advanced = update_line_number(c);
        int col = get_col(c);
        if (state < 0 || state >= N_STATES || col < 0 || col >= N_COLS) {
            recover_from_lexical_error(c, line_advanced);
            state = 0;
            continue;
        }

        sem_act_t action = sem_act_mat[state][col];
        int token_id = action(c);
        int next_state = transition_table[state][col];

        if (token_id == YYerror) {
            begin_lexical_recovery();
            if (next_state == F_RET || (next_state == E && is_valid_token_boundary(c))) {
                push_back_char(c, line_advanced);
            } else if (next_state == E) {
                recover_from_lexical_error(c, line_advanced);
            }
            set_token_location();
            yylval.symbol_ref = NULL;
            return YYerror;
        }

        if (next_state == F_RET)
            push_back_char(c, line_advanced);

        state = next_state;

        if (token_id == -1 && (state == F_CONS || state == F_RET)) {
            state = 0;
            reset_lexeme_buffer();
            continue;
        }

        if (token_id == -1) 
            continue;

        log_token(token_id);
        set_token_location();
        return token_id;
    }
}
