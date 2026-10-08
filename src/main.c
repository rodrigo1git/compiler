#include <stdio.h> 
#include "../include/lexer.h"
#include "../include/semantic_actions.h" 
#include "../include/symbol_table.h"
#include "../y.tab.h"

FILE *source_file = NULL;
int global_errors = 0;
int token_start_line = 1;
int lexical_error_line = 0;
extern int yyparse();

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

    lexer_reset_input();

    // Parse
    init_symbol_table();

    int success = (yyparse() == 0 && global_errors == 0);
    if (success) {
        printf("Parsing successful.\n");
    } else {
        printf("Compilation failed with %d errors.\n", global_errors);
    }

    print_symbol_table();
    map_free(symbol_table);
    free_lexeme_buffer();
    fclose(source_file);
    return success ? 0 : 1;
}

void yyerror(const char *s) {
    fprintf(stderr, "Line %d: %s\n", current_line, s);
}
