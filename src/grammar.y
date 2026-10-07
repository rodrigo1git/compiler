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
    void yyerror(const char *s);
    void add_to_symbol_table(const char* lexeme_buffer, const char* tipo);
    %}
    
    /* Token declarations */
    %union {
      char* str_val;
    }
    %token TOKEN_ID TOKEN_CHAIN
    %token <str_val> TOKEN_CONST
    %token TOKEN_INTEGER TOKEN_SINGLEF
    %token TOKEN_BEGIN TOKEN_END TOKEN_IF TOKEN_END_IF TOKEN_ELSE
    %token TOKEN_FROM TOKEN_TO TOKEN_BY TOKEN_REPEAT
    %token TOKEN_FUNCTION TOKEN_CLASS TOKEN_TOI TOKEN_POUT TOKEN_POUT_LOWER TOKEN_RET
    %token TOKEN_COMPTIME TOKEN_EXTENDS
        /* Relational and assignment operators */
    %token TOKEN_ASSIGN          /* := */
    %token TOKEN_EQUAL           /* == */
    %token TOKEN_NOT_EQUAL       /* != */
    %token TOKEN_LESS_EQUAL      /* <= */
    %token TOKEN_GREATER_EQUAL   /* >= */
    
    %define parse.error custom
    %define parse.lac full

%start statements
    
    %%
    
    /* Grammar rules */
    
    statements:
          program_name decl_list TOKEN_BEGIN statement TOKEN_END ';'
        ;
    
    program_name:
          TOKEN_ID
        | error { fprintf(stderr, "Line %d: Syntax error: Missing program name\n", current_line); yyerrok; }
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
      type id_list ';' { printf("[SYNTAX] Line %d: Variable declaration\n", current_line); }
    | TOKEN_COMPTIME type id_list ';' { printf("[SYNTAX] Line %d: Comptime variable declaration\n", current_line); }
    | type error ';' { yyerrok; }
    | TOKEN_COMPTIME type error ';' { yyerrok; }
    | TOKEN_COMPTIME error id_list ';' { fprintf(stderr, "Line %d: Syntax error: Missing type in comptime declaration\n", current_line); yyerrok; }
    | error ';' { yyerrok; }
    ;
    
    id_list:
          TOKEN_ID
        | id_list ',' TOKEN_ID
        | id_list error TOKEN_ID { fprintf(stderr, "Line %d: Syntax error: Missing ',' in variable declaration\n", current_line); yyerrok; }
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
        | assign error { fprintf(stderr, "Line %d: Syntax error: Missing ';'\n", current_line); yyerrok; }
        | if_stmt
        | for_loop
        | attr_access ';'
        | attr_access error { fprintf(stderr, "Line %d: Syntax error: Missing ';'\n", current_line); yyerrok; }
        | pout_stmt ';'
        | pout_stmt error { fprintf(stderr, "Line %d: Syntax error: Missing ';'\n", current_line); yyerrok; }
        | ret_stmt ';'
        | ret_stmt error { fprintf(stderr, "Line %d: Syntax error: Missing ';'\n", current_line); yyerrok; }
        | error ';' { yyerrok; }
        ;
    
    single_stmt:
          assign ';'
        | assign error { fprintf(stderr, "Line %d: Syntax error: Missing ';'\n", current_line); yyerrok; }
        | if_stmt
        | attr_access ';'
        | attr_access error { fprintf(stderr, "Line %d: Syntax error: Missing ';'\n", current_line); yyerrok; }
        | pout_stmt ';'
        | pout_stmt error { fprintf(stderr, "Line %d: Syntax error: Missing ';'\n", current_line); yyerrok; }
        | ret_stmt ';'
        | ret_stmt error { fprintf(stderr, "Line %d: Syntax error: Missing ';'\n", current_line); yyerrok; }
        | error ';' { yyerrok; }
        ;

    type:
          TOKEN_INTEGER
        | TOKEN_SINGLEF
        | TOKEN_ID
        ;
    
    assign:
          TOKEN_ID TOKEN_ASSIGN expr { printf("[SYNTAX] Line %d: Assignment\n", current_line); }
        | attr_ref TOKEN_ASSIGN expr { printf("[SYNTAX] Line %d: Attribute Assignment\n", current_line); }
        ;
    
    expr:
          expr '+' term
        | expr '-' term
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
    | TOKEN_ID TOKEN_ASSIGN error { fprintf(stderr, "Line %d: Syntax error: Use '=' instead of ':=' for assignments in expressions\n", current_line); yyerrok; }
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
        | TOKEN_ID '(' arg_list ')' { fprintf(stderr, "Line %d: Syntax error: Missing order for parameter evaluation and assignment\n", current_line); }
        | TOKEN_ID '(' error ')' '[' const_list ']' { yyerrok; }
        | TOKEN_ID '(' error ')' { fprintf(stderr, "Line %d: Syntax error: Missing order for parameter evaluation and assignment\n", current_line); yyerrok; }
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
        | single_stmt
        ;

    if_stmt:
          TOKEN_IF '(' cond ')' stmt_block else_stmt TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF '(' cond ')' stmt_block TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF cond ')' stmt_block else_stmt TOKEN_END_IF ';' { fprintf(stderr, "Line %d: Syntax error: Missing opening parenthesis in IF\n", current_line); }
        | TOKEN_IF cond ')' stmt_block TOKEN_END_IF ';' { fprintf(stderr, "Line %d: Syntax error: Missing opening parenthesis in IF\n", current_line); }
        | TOKEN_IF '(' cond stmt_block else_stmt TOKEN_END_IF ';' { fprintf(stderr, "Line %d: Syntax error: Missing closing parenthesis in IF\n", current_line); }
        | TOKEN_IF '(' cond stmt_block TOKEN_END_IF ';' { fprintf(stderr, "Line %d: Syntax error: Missing closing parenthesis in IF\n", current_line); }
        | TOKEN_IF cond stmt_block else_stmt TOKEN_END_IF ';' { fprintf(stderr, "Line %d: Syntax error: Missing opening and closing parentheses in IF\n", current_line); }
        | TOKEN_IF cond stmt_block TOKEN_END_IF ';' { fprintf(stderr, "Line %d: Syntax error: Missing opening and closing parentheses in IF\n", current_line); }
        | TOKEN_IF '(' cond ')' stmt_block else_stmt error ';' { fprintf(stderr, "Line %d: Syntax error: Missing end_if\n", current_line); yyerrok; }
        | TOKEN_IF '(' cond ')' stmt_block error ';' { fprintf(stderr, "Line %d: Syntax error: Missing end_if\n", current_line); yyerrok; }
        ;

    else_stmt:
          TOKEN_ELSE stmt_block
        ;

    for_loop:
          TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block { printf("[SYNTAX] Line %d: FROM-REPEAT loop\n", current_line); }
        | error TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block { fprintf(stderr, "Line %d: Syntax error: Missing 'from' in FOR loop\n", current_line); yyerrok; }
        | TOKEN_FROM error TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block { fprintf(stderr, "Line %d: Syntax error: Missing identifier in FOR loop\n", current_line); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN error TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT stmt_block { fprintf(stderr, "Line %d: Syntax error: Missing constant after 'from' in FOR loop\n", current_line); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant error constant TOKEN_BY constant TOKEN_REPEAT stmt_block { fprintf(stderr, "Line %d: Syntax error: Missing 'to' in FOR loop\n", current_line); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO error TOKEN_BY constant TOKEN_REPEAT stmt_block { fprintf(stderr, "Line %d: Syntax error: Missing constant after 'to' in FOR loop\n", current_line); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant error constant TOKEN_REPEAT stmt_block { fprintf(stderr, "Line %d: Syntax error: Missing 'by' in FOR loop\n", current_line); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY error TOKEN_REPEAT stmt_block { fprintf(stderr, "Line %d: Syntax error: Missing constant after 'by' in FOR loop\n", current_line); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant error stmt_block { fprintf(stderr, "Line %d: Syntax error: Missing 'repeat' in FOR loop\n", current_line); yyerrok; }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT error { fprintf(stderr, "Line %d: Syntax error: Missing body in FOR loop\n", current_line); yyerrok; }
        ;

    func_def:
          type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: Function definition\n", current_line); }
        | type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: Function definition\n", current_line); }
        | type TOKEN_FUNCTION TOKEN_ID '(' error ')' decl_list TOKEN_BEGIN compound_stmt TOKEN_END ';' { yyerrok; }
        | type TOKEN_FUNCTION TOKEN_ID '(' error ')' TOKEN_BEGIN compound_stmt TOKEN_END ';' { yyerrok; }
        | type TOKEN_FUNCTION error '(' param_decl_list ')' decl_list TOKEN_BEGIN compound_stmt TOKEN_END ';' { fprintf(stderr, "Line %d: Syntax error: Missing function name\n", current_line); yyerrok; }
        | type TOKEN_FUNCTION error '(' param_decl_list ')' TOKEN_BEGIN compound_stmt TOKEN_END ';' { fprintf(stderr, "Line %d: Syntax error: Missing function name\n", current_line); yyerrok; }
        ;

    param_decl_list:
          type TOKEN_ID
        | param_decl_list ',' type TOKEN_ID 
        | type error { fprintf(stderr, "Line %d: Syntax error: Missing formal parameter name\n", current_line); yyerrok; }
        | error TOKEN_ID { fprintf(stderr, "Line %d: Syntax error: Missing formal parameter type\n", current_line); yyerrok; }
        | param_decl_list ',' type error { fprintf(stderr, "Line %d: Syntax error: Missing formal parameter name\n", current_line); yyerrok; }
        | param_decl_list ',' error TOKEN_ID { fprintf(stderr, "Line %d: Syntax error: Missing formal parameter type\n", current_line); yyerrok; }
        ;

    method_def:
          type TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: Method definition\n", current_line); }
        | type TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: Method definition\n", current_line); }
        | type TOKEN_ID '(' error ')' decl_list TOKEN_BEGIN compound_stmt TOKEN_END ';' { yyerrok; }
        | type TOKEN_ID '(' error ')' TOKEN_BEGIN compound_stmt TOKEN_END ';' { yyerrok; }
        ;

    class_def:
          TOKEN_CLASS TOKEN_ID TOKEN_ID TOKEN_BEGIN class_body TOKEN_END ';' { printf("[SYNTAX] Line %d: Class declaration (Tema 24)\n", current_line); }
        | TOKEN_CLASS TOKEN_ID TOKEN_ID TOKEN_BEGIN error TOKEN_END ';' { fprintf(stderr, "Line %d: Syntax error: Missing class code\n", current_line); yyerrok; }
        | TOKEN_CLASS TOKEN_ID error TOKEN_END ';' { yyerrok; }
        ;

    class_body:
          class_body class_member
        | class_member
        ;

    extends_stmt:
          TOKEN_EXTENDS id_list ';' { printf("[SYNTAX] Line %d: Extends statement\n", current_line); }
        | TOKEN_EXTENDS error ';' { fprintf(stderr, "Line %d: Syntax error: Missing class list after extends\n", current_line); yyerrok; }
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
        | attr_ref '=' error { yyerrok; }
        ;


    pout_stmt:
          TOKEN_POUT_LOWER '(' expr ')' { printf("[SYNTAX] Line %d: POUT statement\n", current_line); }
        | TOKEN_POUT_LOWER '(' error ')' { fprintf(stderr, "Line %d: Syntax error: Missing argument in pout\n", current_line); yyerrok; }
        ;

    ret_stmt:
          TOKEN_RET '(' expr ')' { printf("[SYNTAX] Line %d: Return statement (RET)\n", current_line); }
        ;

    %%

    static int yyreport_syntax_error(const yypcontext_t *ctx) {
        yysymbol_kind_t unexp = yypcontext_token(ctx);
        const char *unexp_name = yysymbol_name(unexp);



        fprintf(stderr, "Line %d: Syntax error: Unexpected %s", current_line, unexp_name);
        
        int n = yypcontext_expected_tokens(ctx, NULL, 0);
        if (n > 0) {
            yysymbol_kind_t *expected = malloc(n * sizeof(yysymbol_kind_t));
            if (expected != NULL) {
                yypcontext_expected_tokens(ctx, expected, n);
                fprintf(stderr, ", expecting ");
                int limit = (n > 8) ? 8 : n;
                for (int i = 0; i < limit; i++) {
                    if (i > 0) {
                        fprintf(stderr, " or ");
                    }
                    fprintf(stderr, "%s", yysymbol_name(expected[i]));
                }
                if (n > 8) {
                    fprintf(stderr, " ... (and %d more)", n - 8);
                }
                free(expected);
            }
        }
        fprintf(stderr, "\n");
        global_errors++;
        return 0;
    }
