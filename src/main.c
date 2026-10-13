#include <stdio.h> 
#include "../include/lexer.h"
#include "../include/semantic_actions.h" 
#include "../include/symbol_table.h"
#include "../include/diagnostics.h"
#include "../y.tab.h"

FILE *source_file = NULL;
int token_start_line = 1;
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

    int parse_result = yyparse();
    finish_error_recovery();
    map_purge_provisional(symbol_table);
    int success = (parse_result == 0 && global_errors == 0);
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
    finish_error_recovery();
    global_errors++;
    fprintf(stderr, "Line %d: %s\n", current_line, s);
}
