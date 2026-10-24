#include <stdio.h> 
#include "../include/lexer.h"
#include "../include/lexer_actions.h"
#include "../include/symbol_table.h"
#include "../include/diagnostics.h"
#include "../include/scope.h"
#include "../include/codegen.h"
#include <string.h>
#include "../y.tab.h"

FILE *source_file = NULL;
int token_start_line = 1;
extern int yyparse();

int main(int argc, char *argv[]) {
    // Keep traces and diagnostics on separate lines when their streams are merged.
    setvbuf(stdout, NULL, _IOLBF, 0);

    // Validate args
    if (argc < 2) {
        printf("Error: Missing input file.\n");
        return 1;
    }

    bool generate_rpn = true;
    bool trace = false;
    const char *input_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rpn") == 0) generate_rpn = true;
        else if (strcmp(argv[i], "--analyze") == 0) generate_rpn = false;
        else if (strcmp(argv[i], "--trace") == 0) trace = true;
        else if (argv[i][0] == '-' || input_path != NULL) {
            fprintf(stderr, "Usage: %s [--rpn|--analyze] [--trace] input_file\n", argv[0]);
            return 1;
        } else input_path = argv[i];
    }
    if (input_path == NULL) {
        fprintf(stderr, "Error: Missing input file.\n");
        return 1;
    }

    // Open source file
    source_file = fopen(input_path, "r");
    if (!source_file) {
        printf("Error opening file: %s\n", input_path);
        return 1;
    }

    lexer_reset_input();

    // Parse
    init_symbol_table();
    codegen_init();
    set_trace_enabled(trace);
    scope_init();

    int parse_result = yyparse();
    finish_error_recovery();
    if (parse_result == 0 && global_errors == 0) codegen_finalize();
    map_purge_provisional(symbol_table);
    int success = (parse_result == 0 && global_errors == 0);
    if (success) {
        printf("Parsing successful.\n");
        if (generate_rpn) {
            printf("\n--- RPN ---\n");
            if (!codegen_print(stdout)) {
                fprintf(stderr, "Cannot display complete RPN output\n");
                success = 0;
            }
        }
    } else {
        printf("Compilation failed with %d errors.\n", global_errors);
    }

    print_symbol_table();
    codegen_destroy();
    scope_destroy();
    map_free(symbol_table);
    free_lexeme_buffer();
    fclose(source_file);
    return success ? 0 : 1;
}
