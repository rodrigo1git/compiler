#include <stdio.h> 
#include "../include/lexer.h"
#include "../include/semantic_actions.h" 
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

    bool generate_rpn = argc == 3 && strcmp(argv[1], "--rpn") == 0;
    if ((!generate_rpn && argc != 2) || (strcmp(argv[1], "--rpn") == 0 && !generate_rpn)) {
        fprintf(stderr, "Usage: %s [--rpn] input_file\n", argv[0]);
        return 1;
    }
    const char *input_path = argv[generate_rpn ? 2 : 1];

    // Open source file
    source_file = fopen(input_path, "r");
    if (!source_file) {
        printf("Error opening file: %s\n", input_path);
        return 1;
    }

    lexer_reset_input();

    // Parse
    init_symbol_table();
    codegen_init(generate_rpn);
    set_trace_enabled(!generate_rpn);
    scope_init();

    int parse_result = yyparse();
    finish_error_recovery();
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

void yyerror(const char *s) {
    finish_error_recovery();
    global_errors++;
    fprintf(stderr, "Line %d: %s\n", current_line, s);
}
