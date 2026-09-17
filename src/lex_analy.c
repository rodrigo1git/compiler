#include <stdio.h>
#include "../include/stat_trans_mat.h"
#include "../include/sa_mat.h"
#include "../include/lex_analy.h"

extern const char* sem_act_names[15][17]; // Arriba en el archivo

extern YYSTYPE yylval;
extern char lexema[]; // Referencia al buffer global definido en sa_mat.c
extern FILE *s_file;

int state = 0;
int cant_lin = 1;

int yylex(void) {
    char c;
    int tokken = -1;
    int col;

    // Reinicia la máquina de estados para cada token
    state = 0;

    while (tokken == -1) {
        c = fgetc(s_file);
        
        if (c == EOF) {
            return 0;
        }

        if (c == '\n')
          cant_lin++;
        
        // fprintf(stderr, "Caracter leido %c\n", c);
        col = get_col(c);
        
        if (state < 0 || state >= 15 || col < 0 || col >= 17) {
            fprintf(stderr, "[ERROR] Indice fuera de matriz: State=%d, Col=%d con char '%c' (ASCII %d)\n", state, col, c, (int)c); state = 0;
            tokken = -1;
            lex_idx = 0; 
            lexema[0] = '\0';
            // printf("Se descarta el indentificador y se reinicia el estado\n");
        }

        // 2. Error por puntero a función nulo
        sem_act_t sem_act = sem_act_mat[state][col];

        // Ejecución de la acción y cambio de estado
        tokken = sem_act(c);

        // printf("[ACCION] Se ejecutara: %s (State=%d, Col=%d)\n", sem_act_names[state][col], state, col);
        
        state = stat_mat[state][col];

    }
    // Impresión de cada token reconocido junto a su lexema acumulado
    printf("[LEX] Token: %d | Lexema: \"%s\" | En la línea: %d\n", tokken, lexema, cant_lin);

    return tokken;
}
