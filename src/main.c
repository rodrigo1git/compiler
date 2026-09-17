#include <stdio.h> 
#include "../include/lexer.h"
#include "../include/semantic_actions.h" 
#include "../include/symbol_table.h"
#include "../y.tab.h"

FILE *source_file = NULL;
extern int yyparse();
extern int current_line;

int main(int argc, char *argv[]) {
    // Validate args
    if (argc < 2) {
        printf("Error: Missing input file.\n");
        return 1;
    }

    // Open source file
    source_file = fopen(argv[1], "r");
    if (!source_file) {
        printf("Error opening file: %s\n", argv[1]);
        return 1;
    }

    // Parse
    init_symbol_table();

    if (yyparse() == 0) {
        printf("Parsing successful.\n");
    }

    // Dump symbol table
    print_symbol_table();
    fclose(source_file);
    return 0;
}

void yyerror(const char *s) {
    fprintf(stderr, "Line %d: Syntax error: %s (near '%s')\n", current_line, s, lexeme_buffer);
}
