%{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include "include/symbol_table.h"
    
    extern int yylex();
    extern int current_line;
    extern int global_errors;
    extern int lexical_error_line;
    int in_function_depth = 0;
    static const char *recovered_unexpected_name = NULL;
    static char recovered_error_context[256];
    static int recovered_error_context_line = 0;
    static int suppressed_followup_line = 0;
    void yyerror(const char *s);
    map_node_t *add_to_symbol_table(const char* lexeme_buffer, const char* tipo);

    static void clear_recovered_syntax_context(void) {
        recovered_unexpected_name = NULL;
        recovered_error_context[0] = '\0';
        recovered_error_context_line = 0;
    }

    static void report_syntax_error(int line, const char *message) {
        if (suppressed_followup_line != line) {
            suppressed_followup_line = 0;
        }
        if (lexical_error_line == line) {
            lexical_error_line = 0;
            clear_recovered_syntax_context();
            return;
        }
        fprintf(stderr, "Line %d: Syntax error: %s\n", line, message);
        global_errors++;
        if (strstr(message, "Use '=' instead of ':='")) {
            suppressed_followup_line = line;
        }
        clear_recovered_syntax_context();
    }

    static void report_syntax_error_with_context(int line, const char *message) {
        char detailed_message[512];
        if (recovered_error_context_line == line && recovered_error_context[0] != '\0') {
            snprintf(detailed_message, sizeof(detailed_message), "%s (%s)", message, recovered_error_context);
            report_syntax_error(line, detailed_message);
        } else {
            report_syntax_error(line, message);
        }
    }

    static void report_recovered_statement_error(int line, const char *message) {
        if (recovered_unexpected_name != NULL && strcmp(recovered_unexpected_name, "TOKEN_CONST") == 0) {
            if (suppressed_followup_line == line) {
                suppressed_followup_line = 0;
                return;
            }
            report_syntax_error(line, "Missing operator in expression");
        } else if (recovered_unexpected_name != NULL && strcmp(recovered_unexpected_name, "TOKEN_END") == 0) {
            report_syntax_error(line, "Missing ';' after statement");
        } else if (recovered_unexpected_name != NULL && strcmp(recovered_unexpected_name, "TOKEN_CHAIN") == 0) {
            report_syntax_error(line, "String literal cannot be used as an arithmetic operand");
        } else {
            report_syntax_error_with_context(line, message);
        }
    }

    #define SYNERR(location, message) report_syntax_error((location).first_line, (message))
    %}

    %code requires {
      #include "include/symbol_table.h"
    }
    
    /* Token declarations */
    %union {
      map_node_t *symbol_ref;
      int function_scope;
    }
    %token <symbol_ref> TOKEN_ID TOKEN_CHAIN TOKEN_CONST
    %token TOKEN_INTEGER TOKEN_SINGLEF
    %token TOKEN_BEGIN TOKEN_END TOKEN_IF TOKEN_END_IF TOKEN_ELSE
    %token TOKEN_FROM TOKEN_TO TOKEN_BY TOKEN_REPEAT
    %token TOKEN_FUNCTION TOKEN_CLASS TOKEN_TOI TOKEN_POUT TOKEN_POUT_LOWER TOKEN_RET
    %token TOKEN_COMPTIME TOKEN_EXTENDS
    %type <function_scope> function_scope
    %destructor { if ($$) in_function_depth--; } <function_scope>
        /* Relational and assignment operators */
    %token TOKEN_ASSIGN          /* := */
    %token TOKEN_EQUAL           /* == */
    %token TOKEN_NOT_EQUAL       /* != */
    %token TOKEN_LESS_EQUAL      /* <= */
    %token TOKEN_GREATER_EQUAL   /* >= */
    
    %define parse.error custom
    %define parse.lac full
    %locations

%start statements
    
    %%
    
    /* Grammar rules */
    
    statements:
          program_name decl_list TOKEN_BEGIN statement TOKEN_END ';'
        | program_name decl_list TOKEN_BEGIN statement TOKEN_END error { SYNERR(@6, "Missing ';' after program end"); yyerrok; }
        | program_name decl_list TOKEN_BEGIN statement YYEOF { SYNERR(@5, "Missing end"); }
        | program_name decl_list assign ';' TOKEN_END ';' { SYNERR(@3, "Missing begin"); }
        | program_name decl_list assign ';' YYEOF { SYNERR(@3, "Missing begin"); }
        ;
    
    program_name:
          TOKEN_ID
        | error { SYNERR(@$, "Missing program name"); yyerrok; }
        ;
    
    decl_list:
          decl_list var_decl
        | decl_list class_def
        | decl_list func_def
        | var_decl
        | class_def
        | func_def
        ;
    
    var_decl:
      type id_list ';' { printf("[SYNTAX] Line %d: Variable declaration\n", @$.first_line); }
    | TOKEN_COMPTIME type id_list ';' { printf("[SYNTAX] Line %d: Comptime variable declaration\n", @$.first_line); }
    | TOKEN_COMPTIME TOKEN_ID ';' { SYNERR(@1, "Missing type in comptime declaration"); }
    | type error ';' {
          if (recovered_unexpected_name != NULL && strcmp(recovered_unexpected_name, "TOKEN_ASSIGN") == 0)
              SYNERR(@2, "Missing begin");
          else
              SYNERR(@2, "Missing identifier in variable declaration");
          yyerrok;
      }
    | TOKEN_COMPTIME type error ';' { SYNERR(@3, "Missing identifier in comptime declaration"); yyerrok; }
    | TOKEN_COMPTIME error id_list ';' { SYNERR(@2, "Missing type in comptime declaration"); yyerrok; }
    | error ';' { report_syntax_error_with_context(@1.first_line, "Malformed declaration"); yyerrok; }
    ;
    
    id_list:
          TOKEN_ID
        | id_list ',' TOKEN_ID
        | id_list ',' error { SYNERR(@3, "Missing identifier after ',' in variable declaration"); yyerrok; }
        | id_list error TOKEN_ID { SYNERR(@2, "Missing ',' in variable declaration"); yyerrok; }
        ;
    
    statement:
          compound_stmt
        ;
    
    compound_stmt:
          compound_stmt simple_stmt
        | simple_stmt
        | compound_stmt assign { SYNERR(@2, "Missing ';' after assignment"); }
        | assign { SYNERR(@1, "Missing ';' after assignment"); }
        ;
    
    simple_stmt:
          assign ';'
        | if_stmt
        | for_loop
        | attr_access ';'
        | attr_access error { SYNERR(@2, "Missing ';' after attribute assignment"); yyerrok; }
        | pout_stmt ';'
        | pout_stmt error { SYNERR(@2, "Missing ';' after pout statement"); yyerrok; }
        | ret_stmt ';'
        | ret_stmt error { SYNERR(@2, "Missing ';' after ret statement"); yyerrok; }
        | TOKEN_RET '(' expr ';' { SYNERR(@4, "Missing closing parenthesis in ret statement"); }
        | error ';' { report_recovered_statement_error(@1.first_line, "Malformed statement"); yyerrok; }
        ;
    
    single_stmt:
          simple_stmt
        ;

    type:
          primitive_type
        | TOKEN_ID
        ;

    primitive_type:
          TOKEN_INTEGER
        | TOKEN_SINGLEF
        ;
    
    assign:
          TOKEN_ID TOKEN_ASSIGN expr { printf("[SYNTAX] Line %d: Assignment\n", @$.first_line); }
        | attr_ref TOKEN_ASSIGN expr { printf("[SYNTAX] Line %d: Attribute Assignment\n", @$.first_line); }
        ;
    
    expr:
          expr '+' term
        | expr '-' term
        | expr '+' error { SYNERR(@3, "Missing operand after '+'"); yyerrok; }
        | expr '-' error { SYNERR(@3, "Missing operand after '-'"); yyerrok; }
        | term
        ;
    
    term:
          term '*' factor
        | term '/' factor
        | factor
        ;

    constant:
      TOKEN_CONST {
          if (strcmp($1->value, "INTEGER") == 0 && strchr($1->key, '.') == NULL) {
              long val = atol($1->key);
              if (val > 32767) {
                  fprintf(stderr, "Line %d: Lexical error: Positive integer constant '%s' out of range\n", @1.first_line, $1->key);
                  global_errors++;
              }
          }
      }
    | '-' TOKEN_CONST {
          if (strcmp($2->value, "SINGLEF") == 0) {
                  char *neg_str = malloc(strlen($2->key) + 2);
                  sprintf(neg_str, "-%s", $2->key);
                  add_to_symbol_table(neg_str, "SINGLEF");
                  free(neg_str);
          } else if (strcmp($2->value, "INTEGER") == 0) {
              long val = -atol($2->key);
              if (val < -32768) {
                  fprintf(stderr, "Line %d: Lexical error: Negative constant out of range\n", @2.first_line);
                  global_errors++;
              }
              else {
                  char *neg_str = malloc(strlen($2->key) + 2);
                  sprintf(neg_str, "-%s", $2->key);
                  add_to_symbol_table(neg_str, "INTEGER");
                  free(neg_str);
              }
          }
      }
    ;
    

    assign_chain:
      TOKEN_ID '='
    | assign_chain TOKEN_ID '='
    ;

    factor:
      assign_chain TOKEN_ID                                          
    | assign_chain constant                                   
    | assign_chain TOKEN_ID '(' arg_list ')' '[' const_list ']' {
          SYNERR(@2, "Expression assignment with '=' requires an identifier or constant on its right side");
      }
    | assign_chain TOKEN_ID '.' TOKEN_ID '(' arg_list ')' '[' const_list ']' {
          SYNERR(@2, "Expression assignment with '=' requires an identifier or constant on its right side");
      }
    | factor_base 
    | TOKEN_ID TOKEN_ASSIGN error { SYNERR(@2, "Use '=' instead of ':=' for assignments in expressions"); yyerrok; }
    ;
    
    factor_base:
      call
    | TOKEN_ID
    | constant
      | TOKEN_TOI '(' expr ')'
    | TOKEN_TOI '(' ')' { SYNERR(@3, "Missing expression in toi conversion"); }
    | attr_ref
    ;
    
    call:
          TOKEN_ID '(' arg_list ')' '[' const_list ']'
        | TOKEN_ID '(' arg_list ')' { SYNERR(@1, "Missing order for parameter evaluation and assignment"); }
        | TOKEN_ID '.' TOKEN_ID '(' arg_list ')' '[' const_list ']'
        | TOKEN_ID '.' TOKEN_ID '(' arg_list ')' { SYNERR(@1, "Missing order for parameter evaluation and assignment"); }
        | TOKEN_ID '.' TOKEN_ID '(' arg_list ',' ')' '[' const_list ']' { SYNERR(@6, "Missing argument after ',' in method call"); }
        | TOKEN_ID '.' TOKEN_ID '(' error ')' '[' const_list ']' { SYNERR(@5, "Malformed method arguments"); yyerrok; }
        | TOKEN_ID '(' arg_list ',' ')' '[' const_list ']' { SYNERR(@4, "Missing argument after ',' in function call"); }
        | TOKEN_ID '(' error ')' '[' const_list ']' { SYNERR(@3, "Malformed function arguments"); yyerrok; }
        | TOKEN_ID '(' error ')' { SYNERR(@3, "Missing order for parameter evaluation and assignment"); yyerrok; }
        ;
    
    arg_list:
          expr
        | arg_list ',' expr 
        ;
    
    const_list:
          constant
        | const_list ',' constant
        ;
    
    cond:
          expr '<' expr
        | expr '>' expr
        | expr TOKEN_GREATER_EQUAL expr
        | expr TOKEN_LESS_EQUAL expr
        | expr TOKEN_EQUAL expr
        | expr TOKEN_NOT_EQUAL expr
        | expr '<' error { SYNERR(@3, "Missing right operand in condition"); yyerrok; }
        | expr '>' error { SYNERR(@3, "Missing right operand in condition"); yyerrok; }
        | expr TOKEN_GREATER_EQUAL error { SYNERR(@3, "Missing right operand in condition"); yyerrok; }
        | expr TOKEN_LESS_EQUAL error { SYNERR(@3, "Missing right operand in condition"); yyerrok; }
        | expr TOKEN_EQUAL error { SYNERR(@3, "Missing right operand in condition"); yyerrok; }
        | expr TOKEN_NOT_EQUAL error { SYNERR(@3, "Missing right operand in condition"); yyerrok; }
        ;
    
    stmt_block:
          TOKEN_BEGIN compound_stmt TOKEN_END
        | TOKEN_BEGIN compound_stmt YYEOF { SYNERR(@3, "Missing end"); }
        | single_stmt
        ;

    if_stmt:
          TOKEN_IF '(' cond ')' stmt_block else_stmt TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", @$.first_line); }
        | TOKEN_IF '(' cond ')' stmt_block TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", @$.first_line); }
        | TOKEN_IF cond ')' stmt_block else_stmt TOKEN_END_IF ';' { SYNERR(@1, "Missing opening parenthesis in IF"); }
        | TOKEN_IF cond ')' stmt_block TOKEN_END_IF ';' { SYNERR(@1, "Missing opening parenthesis in IF"); }
        | TOKEN_IF '(' cond stmt_block else_stmt TOKEN_END_IF ';' { SYNERR(@1, "Missing closing parenthesis in IF"); }
        | TOKEN_IF '(' cond stmt_block TOKEN_END_IF ';' { SYNERR(@1, "Missing closing parenthesis in IF"); }
        | TOKEN_IF cond stmt_block else_stmt TOKEN_END_IF ';' { SYNERR(@1, "Missing opening and closing parentheses in IF"); }
        | TOKEN_IF cond stmt_block TOKEN_END_IF ';' { SYNERR(@1, "Missing opening and closing parentheses in IF"); }
        | TOKEN_IF '(' cond ')' stmt_block error { SYNERR(@6, "Missing end_if"); yyerrok; }
        | TOKEN_IF '(' cond ')' stmt_block else_stmt error { SYNERR(@7, "Missing end_if"); yyerrok; }
        ;

    else_stmt:
          TOKEN_ELSE stmt_block
        ;

    for_loop:
          TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block ';' { printf("[SYNTAX] Line %d: FROM-REPEAT loop\n", @1.first_line); }
        | TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block ';' { SYNERR(@1, "Missing 'from' in FOR loop"); }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT ';' { SYNERR(@1, "Missing body in FOR loop"); }
        | error TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block ';' { SYNERR(@1, "Missing 'from' in FOR loop"); yyerrok; }
        | TOKEN_FROM error TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block ';' { SYNERR(@2, "Missing identifier in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN error TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block ';' { SYNERR(@4, "Missing constant after 'from' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant error constant TOKEN_BY constant TOKEN_REPEAT stmt_block ';' { SYNERR(@5, "Missing 'to' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO error TOKEN_BY constant TOKEN_REPEAT stmt_block ';' { SYNERR(@6, "Missing constant after 'to' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant error constant TOKEN_REPEAT stmt_block ';' { SYNERR(@7, "Missing 'by' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY error TOKEN_REPEAT stmt_block ';' { SYNERR(@8, "Missing constant after 'by' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant error stmt_block ';' { SYNERR(@9, "Missing 'repeat' in FOR loop"); yyerrok; }
        ;

    func_def:
          type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $9 = 0; in_function_depth--; printf("[SYNTAX] Line %d: Function definition\n", @2.first_line); }
        | type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $8 = 0; in_function_depth--; printf("[SYNTAX] Line %d: Function definition\n", @2.first_line); }
        | type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN function_scope compound_stmt YYEOF { $9 = 0; in_function_depth--; SYNERR(@10, "Missing end of function"); }
        | type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN function_scope compound_stmt YYEOF { $8 = 0; in_function_depth--; SYNERR(@9, "Missing end of function"); }
        | type TOKEN_FUNCTION TOKEN_ID '(' error ')' decl_list TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $9 = 0; in_function_depth--; SYNERR(@4, "Malformed formal parameter list"); yyerrok; }
        | type TOKEN_FUNCTION TOKEN_ID '(' error ')' TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $8 = 0; in_function_depth--; SYNERR(@4, "Malformed formal parameter list"); yyerrok; }
        | type TOKEN_FUNCTION error '(' param_decl_list ')' decl_list TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $9 = 0; in_function_depth--; SYNERR(@3, "Missing function name"); yyerrok; }
        | type TOKEN_FUNCTION error '(' param_decl_list ')' TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $8 = 0; in_function_depth--; SYNERR(@3, "Missing function name"); yyerrok; }
        ;

    function_scope:
          %empty { in_function_depth++; $$ = 1; }
        ;

    param_decl_list:
          type TOKEN_ID
        | param_decl_list ',' type TOKEN_ID 
        | primitive_type error { SYNERR(@2, "Missing formal parameter name"); yyerrok; }
        | error TOKEN_ID { SYNERR(@1, "Missing formal parameter type"); yyerrok; }
        | param_decl_list ',' primitive_type error { SYNERR(@4, "Missing formal parameter name"); yyerrok; }
        | param_decl_list ',' error TOKEN_ID { SYNERR(@3, "Missing formal parameter type"); yyerrok; }
        ;

    method_def:
          type TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $8 = 0; in_function_depth--; printf("[SYNTAX] Line %d: Method definition\n", @2.first_line); }
        | type TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $7 = 0; in_function_depth--; printf("[SYNTAX] Line %d: Method definition\n", @2.first_line); }
        | type TOKEN_ID '(' error ')' decl_list TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $8 = 0; in_function_depth--; SYNERR(@3, "Malformed formal parameter list"); yyerrok; }
        | type TOKEN_ID '(' error ')' TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $7 = 0; in_function_depth--; SYNERR(@3, "Malformed formal parameter list"); yyerrok; }
        ;

    class_def:
          TOKEN_CLASS TOKEN_ID TOKEN_ID TOKEN_BEGIN class_body TOKEN_END ';' { printf("[SYNTAX] Line %d: Class declaration (Tema 24)\n", @1.first_line); }
        | TOKEN_CLASS TOKEN_ID TOKEN_BEGIN class_body TOKEN_END ';' { SYNERR(@1, "Missing class code"); }
        | TOKEN_CLASS TOKEN_ID TOKEN_ID error TOKEN_END ';' { SYNERR(@4, "Missing 'begin' in class declaration"); yyerrok; }
        | TOKEN_CLASS TOKEN_ID TOKEN_ID TOKEN_BEGIN error TOKEN_END ';' { SYNERR(@5, "Malformed class body"); yyerrok; }
        | TOKEN_CLASS TOKEN_ID error TOKEN_END ';' { SYNERR(@3, "Malformed class declaration"); yyerrok; }
        ;

    class_body:
          class_body class_member
        | class_member
        ;

    extends_stmt:
          TOKEN_EXTENDS extends_id_list ';' { printf("[SYNTAX] Line %d: Extends statement\n", @$.first_line); }
        | TOKEN_EXTENDS extends_id_list ',' error ';' { SYNERR(@4, "Missing class name after ',' in extends list"); yyerrok; }
        | TOKEN_EXTENDS error ';' { SYNERR(@2, "Missing class list after extends"); yyerrok; }
        ;

    extends_id_list:
          TOKEN_ID
        | extends_id_list ',' TOKEN_ID
        ;
    
    class_member:
          var_decl
        | method_def
        | extends_stmt
        ;

    attr_ref:
          TOKEN_ID '[' constant ']'
        | TOKEN_ID '[' TOKEN_ID ']'
        ;

    attr_access:
          attr_ref '=' expr
        | attr_ref '=' error { SYNERR(@3, "Missing expression in attribute assignment"); yyerrok; }
        ;


    pout_stmt:
          TOKEN_POUT_LOWER '(' expr ')' { printf("[SYNTAX] Line %d: POUT statement\n", @$.first_line); }
        | TOKEN_POUT_LOWER '(' TOKEN_CHAIN ')' { printf("[SYNTAX] Line %d: POUT statement\n", @$.first_line); }
        | TOKEN_POUT_LOWER '(' error ')' { SYNERR(@3, "Missing argument in pout"); yyerrok; }
        ;

    ret_stmt:
          TOKEN_RET '(' ')' { SYNERR(@3, "Missing expression in ret statement"); }
        | TOKEN_RET '(' expr ')' {
              if (in_function_depth <= 0) {
                  SYNERR(@1, "Forbidden return (Outside function context)");
              } else {
                  printf("[SYNTAX] Line %d: Return statement (RET)\n", @1.first_line);
              }
          }
        | TOKEN_RET '(' expr error { SYNERR(@4, "Missing closing parenthesis in ret statement"); yyerrok; }
        ;

    %%

    static const char *friendly_symbol_name(yysymbol_kind_t symbol);

    static int yyreport_syntax_error(const yypcontext_t *ctx) {
        enum { EXPECTED_DISPLAY_LIMIT = 3 };
        yysymbol_kind_t expected[EXPECTED_DISPLAY_LIMIT];
        yysymbol_kind_t unexpected = yypcontext_token(ctx);
        YYLTYPE *location = yypcontext_location(ctx);
        int expected_count;
        int shown_count;
        size_t used;

        recovered_unexpected_name = yysymbol_name(unexpected);
        recovered_error_context_line = location != NULL ? location->first_line : current_line;
        recovered_error_context[0] = '\0';

        if (unexpected != YYSYMBOL_YYEMPTY) {
            snprintf(recovered_error_context, sizeof(recovered_error_context),
                     "unexpected %s", friendly_symbol_name(unexpected));
        }

        expected_count = yypcontext_expected_tokens(ctx, NULL, 0);
        if (expected_count < 0) {
            return expected_count;
        }
        if (expected_count > 0) {
            int result = yypcontext_expected_tokens(ctx, expected, EXPECTED_DISPLAY_LIMIT);
            if (result < 0) {
                return result;
            }
            shown_count = expected_count < EXPECTED_DISPLAY_LIMIT ? expected_count : EXPECTED_DISPLAY_LIMIT;
            used = strlen(recovered_error_context);
            if (used < sizeof(recovered_error_context)) {
                snprintf(recovered_error_context + used, sizeof(recovered_error_context) - used,
                         "%sexpected ", used == 0 ? "" : "; ");
            }
            for (int i = 0; i < shown_count && used < sizeof(recovered_error_context); i++) {
                used = strlen(recovered_error_context);
                snprintf(recovered_error_context + used, sizeof(recovered_error_context) - used,
                         "%s%s", i == 0 ? "" : (i == shown_count - 1 ? " or " : ", "),
                         friendly_symbol_name(expected[i]));
            }
            if (expected_count > EXPECTED_DISPLAY_LIMIT && used < sizeof(recovered_error_context)) {
                used = strlen(recovered_error_context);
                snprintf(recovered_error_context + used, sizeof(recovered_error_context) - used,
                         " or another valid token");
            }
        }
        return 0;
    }

    static const char *friendly_symbol_name(yysymbol_kind_t symbol) {
        const char *name = yysymbol_name(symbol);
        if (strcmp(name, "TOKEN_ID") == 0) return "identifier";
        if (strcmp(name, "TOKEN_CONST") == 0) return "constant";
        if (strcmp(name, "TOKEN_CHAIN") == 0) return "string literal";
        if (strcmp(name, "TOKEN_INTEGER") == 0) return "'integer'";
        if (strcmp(name, "TOKEN_SINGLEF") == 0) return "'singlef'";
        if (strcmp(name, "TOKEN_BEGIN") == 0) return "'begin'";
        if (strcmp(name, "TOKEN_END") == 0) return "'end'";
        if (strcmp(name, "TOKEN_IF") == 0) return "'if'";
        if (strcmp(name, "TOKEN_END_IF") == 0) return "'end_if'";
        if (strcmp(name, "TOKEN_ELSE") == 0) return "'else'";
        if (strcmp(name, "TOKEN_ASSIGN") == 0) return "':='";
        if (strcmp(name, "TOKEN_EQUAL") == 0) return "'=='";
        if (strcmp(name, "TOKEN_NOT_EQUAL") == 0) return "'!='";
        if (strcmp(name, "TOKEN_LESS_EQUAL") == 0) return "'<='";
        if (strcmp(name, "TOKEN_GREATER_EQUAL") == 0) return "'>='";
        if (strcmp(name, "TOKEN_FUNCTION") == 0) return "'function'";
        if (strcmp(name, "TOKEN_CLASS") == 0) return "'class'";
        if (strcmp(name, "TOKEN_TOI") == 0) return "'toi'";
        if (strcmp(name, "TOKEN_POUT") == 0 || strcmp(name, "TOKEN_POUT_LOWER") == 0) return "'pout'";
        if (strcmp(name, "TOKEN_RET") == 0) return "'ret'";
        if (strcmp(name, "TOKEN_COMPTIME") == 0) return "'comptime'";
        if (strcmp(name, "TOKEN_FROM") == 0) return "'from'";
        if (strcmp(name, "TOKEN_TO") == 0) return "'to'";
        if (strcmp(name, "TOKEN_BY") == 0) return "'by'";
        if (strcmp(name, "TOKEN_REPEAT") == 0) return "'repeat'";
        if (strcmp(name, "TOKEN_EXTENDS") == 0) return "'extends'";
        if (symbol == YYSYMBOL_YYEOF) return "end of file";
        return name;
    }
