 %{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    
    extern int yylex();
    extern int current_line;
    extern int lex_range_reported;
    extern int token_start_line;
    extern int global_errors;
    extern int prev_token;
    int in_function_depth = 0;
    static int recovered_unexpected_token = 0;
    void yyerror(const char *s);
    void add_to_symbol_table(const char* lexeme_buffer, const char* tipo);

    static void report_syntax_error(int line, const char *message) {
        fprintf(stderr, "Line %d: Syntax error: %s\n", line, message);
        global_errors++;
    }

    #define SYNERR(location, message) report_syntax_error((location).first_line, (message))
    %}
    
    /* Token declarations */
    %union {
      char* str_val;
      int function_scope;
    }
    %token TOKEN_ID TOKEN_CHAIN
    %token <str_val> TOKEN_CONST
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
          if (recovered_unexpected_token == YYSYMBOL_TOKEN_ASSIGN)
              SYNERR(@2, "Missing begin");
          else
              SYNERR(@2, "Missing identifier in variable declaration");
          yyerrok;
      }
    | TOKEN_COMPTIME type error ';' { SYNERR(@3, "Missing identifier in comptime declaration"); yyerrok; }
    | TOKEN_COMPTIME error id_list ';' { SYNERR(@2, "Missing type in comptime declaration"); yyerrok; }
    | error ';' { SYNERR(@1, "Malformed declaration"); yyerrok; }
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
        ;
    
    simple_stmt:
          assign ';'
        | assign error { SYNERR(@2, "Missing ';' after assignment"); yyerrok; }
        | if_stmt
        | for_loop
        | attr_access ';'
        | attr_access error { SYNERR(@2, "Missing ';' after attribute assignment"); yyerrok; }
        | pout_stmt ';'
        | pout_stmt error { SYNERR(@2, "Missing ';' after pout statement"); yyerrok; }
        | ret_stmt ';'
        | ret_stmt error { SYNERR(@2, "Missing ';' after ret statement"); yyerrok; }
        | error ';' {
              if (recovered_unexpected_token == YYSYMBOL_TOKEN_CONST)
                  SYNERR(@1, "Missing operator in expression");
              else if (recovered_unexpected_token == YYSYMBOL_TOKEN_END)
                  SYNERR(@1, "Missing ';' after statement");
              else
                  SYNERR(@1, "Malformed statement");
              yyerrok;
          }
        ;
    
    single_stmt:
          assign ';'
        | assign error { SYNERR(@2, "Missing ';' after assignment"); yyerrok; }
        | if_stmt
        | attr_access ';'
        | attr_access error { SYNERR(@2, "Missing ';' after attribute assignment"); yyerrok; }
        | pout_stmt ';'
        | pout_stmt error { SYNERR(@2, "Missing ';' after pout statement"); yyerrok; }
        | ret_stmt ';'
        | ret_stmt error { SYNERR(@2, "Missing ';' after ret statement"); yyerrok; }
        | error ';' {
              if (recovered_unexpected_token == YYSYMBOL_TOKEN_CONST)
                  SYNERR(@1, "Missing operator in expression");
              else if (recovered_unexpected_token == YYSYMBOL_TOKEN_END)
                  SYNERR(@1, "Missing ';' after statement");
              else
                  SYNERR(@1, "Malformed statement");
              yyerrok;
          }
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
          int ya = lex_range_reported;
          lex_range_reported = 0;
          if (!ya && strchr($1, '.') == NULL && strchr($1, 'e') == NULL) {
              long val = atol($1);
              if (val > 32767) {
                  fprintf(stderr, "Line %d: Lexical error: Positive constant out of range\n", token_start_line);
                  global_errors++;
              }
          }
      }
    | '-' TOKEN_CONST {
          int ya = lex_range_reported;
          lex_range_reported = 0;
          int parser_error = 0;
          
          if (strchr($2, '.') != NULL || strchr($2, 'e') != NULL) {
              if (!ya) {
                  char *neg_str = malloc(strlen($2) + 2);
                  sprintf(neg_str, "-%s", $2);
                  add_to_symbol_table(neg_str, "SINGLEF");
                  free(neg_str);
              }
          } else {
              long val = -atol($2);
              if (!ya && val < -32768) {
                  fprintf(stderr, "Line %d: Lexical error: Negative constant out of range\n", token_start_line);
                  global_errors++;
                  parser_error = 1;
              }
              
              if (!ya && !parser_error) {
                  char *neg_str = malloc(strlen($2) + 2);
                  sprintf(neg_str, "-%s", $2);
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
    | factor_base 
    | TOKEN_ID TOKEN_ASSIGN error { SYNERR(@2, "Use '=' instead of ':=' for assignments in expressions"); yyerrok; }
    ;
    
    factor_base:
      call
    | TOKEN_ID
    | constant
    | TOKEN_CHAIN
    | TOKEN_TOI '(' expr ')'
    | attr_ref
    ;
    
    call:
          TOKEN_ID '(' arg_list ')' '[' const_list ']'
        | TOKEN_ID '(' arg_list ')' { SYNERR(@1, "Missing order for parameter evaluation and assignment"); }
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
        | TOKEN_IF '(' cond ')' stmt_block else_stmt error ';' { SYNERR(@6, "Missing end_if"); yyerrok; }
        | TOKEN_IF '(' cond ')' stmt_block error ';' { SYNERR(@5, "Missing end_if"); yyerrok; }
        ;

    else_stmt:
          TOKEN_ELSE stmt_block
        ;

    for_loop:
          TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block { printf("[SYNTAX] Line %d: FROM-REPEAT loop\n", @1.first_line); }
        | TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block { SYNERR(@1, "Missing 'from' in FOR loop"); }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT ';' { SYNERR(@1, "Missing body in FOR loop"); }
        | error TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block { SYNERR(@1, "Missing 'from' in FOR loop"); yyerrok; }
        | TOKEN_FROM error TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block { SYNERR(@2, "Missing identifier in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN error TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block { SYNERR(@4, "Missing constant after 'from' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant error constant TOKEN_BY constant TOKEN_REPEAT stmt_block { SYNERR(@5, "Missing 'to' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO error TOKEN_BY constant TOKEN_REPEAT stmt_block { SYNERR(@6, "Missing constant after 'to' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant error constant TOKEN_REPEAT stmt_block { SYNERR(@7, "Missing 'by' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY error TOKEN_REPEAT stmt_block { SYNERR(@8, "Missing constant after 'by' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant error stmt_block { SYNERR(@9, "Missing 'repeat' in FOR loop"); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT error { SYNERR(@10, "Missing body in FOR loop"); yyerrok; }
        ;

    func_def:
          type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $9 = 0; in_function_depth--; printf("[SYNTAX] Line %d: Function definition\n", @2.first_line); }
        | type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN function_scope compound_stmt TOKEN_END ';' { $8 = 0; in_function_depth--; printf("[SYNTAX] Line %d: Function definition\n", @2.first_line); }
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
        | TOKEN_POUT_LOWER '(' error ')' { SYNERR(@3, "Missing argument in pout"); yyerrok; }
        ;

    ret_stmt:
          TOKEN_RET '(' expr ')' {
              if (in_function_depth <= 0) {
                  SYNERR(@1, "Forbidden return (Outside function context)");
              } else {
                  printf("[SYNTAX] Line %d: Return statement (RET)\n", @1.first_line);
              }
          }
        ;

    %%

    static int yyreport_syntax_error(const yypcontext_t *ctx) {
        /* Recovery productions own user-facing diagnostics and accounting. */
        recovered_unexpected_token = (int)yypcontext_token(ctx);
        (void)yypcontext_expected_tokens(ctx, NULL, 0);
        return 0;
    }
