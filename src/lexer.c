#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>

#include "../include/lexer.h"
#include "../include/semantic_actions.h"
#include "../include/transition_table.h"

int current_line = 1;

typedef struct {
    int previous_char;
    bool eof_delimiter_sent;
} lexer_input_state_t;

static lexer_input_state_t input_state;

static int read_input_char(bool *is_synthetic) {
    int c = fgetc(source_file);
    *is_synthetic = false;

    if (c == EOF && !input_state.eof_delimiter_sent) {
        input_state.eof_delimiter_sent = true;
        *is_synthetic = true;
        return '\n';
    }

    return c;
}

static int update_line_number(int c, bool is_synthetic) {
    int advanced = 0;

    if (is_synthetic) {
        return advanced;
    }

    if (c == '\n' && input_state.previous_char != '\r') {
        current_line++;
        advanced = 1;
    } else if (c == '\r') {
        current_line++;
        advanced = 1;
    }

    input_state.previous_char = c;
    return advanced;
}

static bool is_recovery_delimiter(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';';
}

static bool is_valid_token_boundary(int c) {
    return get_col(c) != COL_OTHER && !isalnum((unsigned char)c) && c != '_' && c != '$';
}

static void push_back_char(int c, int line_advanced) {
    if (line_advanced) {
        current_line--;
    }

    if (c != EOF) {
        (void)ungetc(c, source_file);
        input_state.previous_char = '\0';
    }
}

static void recover_from_lexical_error(int c, bool is_synthetic, int line_advanced) {
    while (!is_recovery_delimiter(c) && c != EOF) {
        c = read_input_char(&is_synthetic);
        line_advanced = update_line_number(c, is_synthetic);
    }

    if (c != EOF) {
        push_back_char(is_synthetic ? EOF : c, line_advanced);
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
    printf("[LEX] Token: %d | Lexeme: \"%s\" | Line: %d\n",
           token_id, lexeme_buffer, token_start_line);
}

void lexer_reset_input(void) {
    input_state.previous_char = '\0';
    input_state.eof_delimiter_sent = false;
    current_line = 1;
    token_start_line = 1;
    lexical_error_line = 0;
    reset_lexeme_buffer();
}

int yylex(void) {
    int state = 0;

    for (;;) {
        if (state == 0) {
            token_start_line = current_line;
        }

        bool is_synthetic = false;
        int c = read_input_char(&is_synthetic);

        if (c == EOF) {
            if (state == ST_CHAIN) {
                fprintf(stderr, "Line %d: Lexical error: Unclosed string literal\n", token_start_line);
                global_errors++;
            } else if (state == E) {
                fprintf(stderr, "Line %d: Lexical error: Unrecognized symbol or invalid sequence.\n", current_line);
                global_errors++;
            } else if (state != 0) {
                fprintf(stderr, "Line %d: Lexical error: Unexpected end of file within token\n", token_start_line);
                global_errors++;
            }

            set_token_location();
            return 0;
        }

        int line_advanced = update_line_number(c, is_synthetic);
        int col = get_col(c);
        if (state < 0 || state >= N_STATES || col < 0 || col >= N_COLS) {
            recover_from_lexical_error(c, is_synthetic, line_advanced);
            state = 0;
            continue;
        }

        sem_act_t action = sem_act_mat[state][col];
        int token_id = action(c);
        int next_state = transition_table[state][col];

        if (next_state == E && token_id == -1 && is_valid_token_boundary(c)) {
            if (!is_synthetic) {
                push_back_char(c, line_advanced);
            }
            state = 0;
            reset_lexeme_buffer();
            continue;
        }

        if (next_state == F_RET) {
            if (!is_synthetic) {
                push_back_char(c, line_advanced);
            }
        }

        state = next_state;

        if (token_id == -1 && (state == F_CONS || state == F_RET)) {
            state = 0;
            reset_lexeme_buffer();
            continue;
        }

        if (token_id == -1) {
            continue;
        }

        log_token(token_id);
        if (lexical_error_line != 0 && token_start_line != lexical_error_line) {
            lexical_error_line = 0;
        }
        set_token_location();
        return token_id;
    }
}
