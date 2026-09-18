#include <stdio.h>
#include "../include/transition_table.h"
#include "../include/semantic_actions.h"
#include "../include/lexer.h"

extern const char* sem_act_names[15][17];

extern YYSTYPE yylval;
extern char lexeme_buffer[];
extern FILE *source_file;

int state = 0;
int current_line = 1;

int yylex(void) {
    char c;
    int token_id = -1;
    int col;

    state = 0;

    while (token_id == -1) {
        c = fgetc(source_file);
        
        if (c == EOF) {
            return 0;
        }

        if (c == '\n')
          current_line++;
        
        col = get_col(c);
        
        if (state < 0 || state >= 15 || col < 0 || col >= 17) {
            fprintf(stderr, "Line %d: Lexical error: Unrecognized symbol or invalid sequence.\n", current_line);
            
            // --- MODO PÁNICO LÉXICO ---
            // Consumimos caracteres hasta encontrar un delimitador limpio
            while (c != ' ' && c != '\t' && c != '\n' && c != ';' && c != EOF) {
                c = fgetc(source_file);
            }
            // Si el delimitador salvavidas no es EOF, lo devolvemos para que se lea en el siguiente ciclo
            if (c != EOF) {
                ungetc(c, source_file);
            }
            
            state = 0;
            token_id = -1;
            lexeme_length = 0; 
            lexeme_buffer[0] = '\0';
            continue;
        }

        sem_act_t sem_act = sem_act_mat[state][col];

        token_id = sem_act(c);

        state = transition_table[state][col];

        if (token_id == -1 && state == F) {
            state = 0;
            lexeme_length = 0; 
            lexeme_buffer[0] = '\0';
        }

    }

    printf("[LEX] Token: %d | Lexeme: \"%s\" | Line: %d\n", token_id, lexeme_buffer, current_line);

    return token_id;
}
