#include <stdio.h> 
#include "../include/lex_analy.h"
#include "../include/sa_mat.h" 
#include "../include/map.h"
#include "../y.tab.h"

FILE *s_file = NULL;
extern int yyparse();
extern int cant_lin;

int main(int argc, char *argv[]) {
    // 1. Validar argumentos
    if (argc < 2) {
        printf("Error: Falta indicar el archivo de entrada.\n");
        return 1;
    }

    // 2. Abrir el archivo pasado por consola
    s_file = fopen(argv[1], "r");
    if (!s_file) {
        printf("Error al abrir el archivo: %s\n", argv[1]);
        return 1;
    }

    // 3. Inicializar e invocar el parser
    init_symbol_table();

    if (yyparse() == 0) {
        printf("Análisis sintáctico exitoso.\n");
    }

    // 4. Imprimir tabla y cerrar
    print_symbol_table();
    fclose(s_file);
    return 0;
}

void yyerror(const char *s) {
    fprintf(stderr, "Error en la línea %d, lexema: %s\n", cant_lin, lexema);
}
