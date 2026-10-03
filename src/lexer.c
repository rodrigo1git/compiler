#include <stdio.h>
#include "../include/transition_table.h"
#include "../include/semantic_actions.h"
#include "../include/lexer.h"

extern const char* sem_act_names[15][17];

extern YYSTYPE yylval;
extern char *lexeme_buffer;
extern FILE *source_file;

int state = 0;
int current_line = 1;
int prev_token = 0;
int cur_token = 0;

int yylex(void) {
    char c;
    int token_id = -1;
    int col;
    static char last_c = '\0';
    static int eof_padded = 0;
    int token_start_line = current_line;

    state = 0;

    while (token_id == -1) {
        if (state == 0) {
            token_start_line = current_line;
        }

        c = fgetc(source_file);

        if (c == EOF && !eof_padded) {
            c = '\n';
            eof_padded = 1;
        }

        if (c == EOF) {
            if (state == ST_CHAIN) {
                fprintf(stderr, "Line %d: Lexical error: Unclosed string literal\n", token_start_line);
            } else if (state == E) {
                fprintf(stderr, "Line %d: Lexical error: Unrecognized symbol or invalid sequence.\n", current_line);
            } else if (state != 0) {
                fprintf(stderr, "Line %d: Lexical error: Unexpected end of file within token\n", token_start_line);
            }
            prev_token = cur_token;
            cur_token = 0;
            return 0;
        }

        int incremented = 0;
        if (c == '\n') {
            if (last_c != '\r') {
                current_line++;
                incremented = 1;
            }
        } else if (c == '\r') {
            current_line++;
            incremented = 1;
        }
        last_c = c;
        
        col = get_col(c);
        
        if (state < 0 || state >= 15 || col < 0 || col >= 17) {
            fprintf(stderr, "Line %d: Lexical error: Unrecognized symbol or invalid sequence.\n", current_line);
            
            while (c != ' ' && c != '\t' && c != '\n' && c != ';' && c != EOF) {
                c = fgetc(source_file);
            }
            if (c != EOF) {
                ungetc(c, source_file);
            }
            
            state = 0;
            token_id = -1;
            reset_lexeme_buffer();
            continue;
        }

        sem_act_t sem_act = sem_act_mat[state][col];

        token_id = sem_act(c);

        int next_state = transition_table[state][col];

        if (next_state == F_RET) {
            if (c != EOF) {
                if (incremented) current_line--;
                ungetc(c, source_file);
                last_c = '\0';
            }
        }

        state = next_state;

        if (token_id == -1 && (state == F_CONS || state == F_RET)) {
            state = 0;
            reset_lexeme_buffer();
        }

    }

    printf("[LEX] Token: %d | Lexeme: \"%s\" | Line: %d\n", token_id, lexeme_buffer, current_line);

    prev_token = cur_token;
    cur_token = token_id;

    return token_id;
}
