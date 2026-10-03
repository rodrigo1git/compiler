 %{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    
    extern int yylex();
    extern int current_line;
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
    | error ';' { yyerrok; }
    ;
    
    id_list:
          TOKEN_ID
        | id_list ',' TOKEN_ID
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
        | if_stmt
        | for_loop
        | attr_access ';'
        | pout_stmt ';'
        | error ';' { yyerrok; }
        ;
    
    single_stmt:
          assign ';'
        | if_stmt
        | attr_access ';'
        | pout_stmt ';'
        | error ';' { yyerrok; }
        ;

    func_compound_stmt:                                                                       
          func_compound_stmt func_simple_stmt                                                 
        | func_simple_stmt                                                                    
        ;
    
    func_simple_stmt:                                                                         
          assign ';'                                                                          
        | func_if_stmt                          
        | func_for_loop
        | attr_access ';'                                                                     
        | pout_stmt ';'                                                                       
        | ret_stmt ';'
        | error ';' { yyerrok; }                                                              
        ;  

    func_single_stmt:                                                                         
          assign ';'                                                                          
        | func_if_stmt  
        | attr_access ';'                                                                     
        | pout_stmt ';'                                                                       
        | ret_stmt ';'                              
        | error ';' { yyerrok; }                                                              
        ;                      

    type:
          TOKEN_INTEGER
        | TOKEN_SINGLEF
        | TOKEN_ID
        ;
    
    assign:
          TOKEN_ID TOKEN_ASSIGN expr { printf("[SYNTAX] Line %d: Assignment\n", current_line); }
        | TOKEN_ID TOKEN_ASSIGN error { yyerrok; }
        | attr_ref TOKEN_ASSIGN expr { printf("[SYNTAX] Line %d: Attribute Assignment\n", current_line); }
        | attr_ref TOKEN_ASSIGN error { yyerrok; }
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
          if (strchr($1, '.') == NULL && strchr($1, 'e') == NULL) {
              long val = atol($1);
              if (val > 32767) {
                  fprintf(stderr, "Line %d: Lexical error: Positive constant out of range\n", current_line);
                  YYERROR;
              }
          }
      }
    | '-' TOKEN_CONST {
          if (strchr($2, '.') != NULL || strchr($2, 'e') != NULL) {
              char *neg_str = malloc(strlen($2) + 2);
              sprintf(neg_str, "-%s", $2);
              add_to_symbol_table(neg_str, "SINGLEF");
              free(neg_str);
          } else {
              long val = -atol($2);
              if (val < -32768) {
                  fprintf(stderr, "Line %d: Lexical error: Negative constant out of range\n", current_line);
                  YYERROR;
              } else {
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
        | TOKEN_ID '(' error ')' '[' const_list ']' { yyerrok; }
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
    
    if_stmt:
          TOKEN_IF '(' cond ')' TOKEN_BEGIN compound_stmt TOKEN_END else_stmt TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF '(' cond ')' TOKEN_BEGIN compound_stmt TOKEN_END TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF '(' cond ')' single_stmt else_stmt TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF '(' cond ')' single_stmt TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF '(' error ')' TOKEN_BEGIN compound_stmt TOKEN_END else_stmt TOKEN_END_IF ';' { yyerrok; }
        | TOKEN_IF '(' error ')' TOKEN_BEGIN compound_stmt TOKEN_END TOKEN_END_IF ';' { yyerrok; }
        | TOKEN_IF '(' error ')' single_stmt else_stmt TOKEN_END_IF ';' { yyerrok; }
        | TOKEN_IF '(' error ')' single_stmt TOKEN_END_IF ';' { yyerrok; }
        ;

    else_stmt:
          TOKEN_ELSE TOKEN_BEGIN compound_stmt TOKEN_END
        | TOKEN_ELSE single_stmt
        ;

    for_loop:
          TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT TOKEN_BEGIN compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: FROM-REPEAT loop\n", current_line); }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT single_stmt { printf("[SYNTAX] Line %d: FROM-REPEAT loop\n", current_line); }
        ;

    func_if_stmt:
          TOKEN_IF '(' cond ')' TOKEN_BEGIN func_compound_stmt TOKEN_END func_else_stmt TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF '(' cond ')' TOKEN_BEGIN func_compound_stmt TOKEN_END TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF '(' cond ')' func_single_stmt func_else_stmt TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF '(' cond ')' func_single_stmt TOKEN_END_IF ';' { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        | TOKEN_IF '(' error ')' TOKEN_BEGIN func_compound_stmt TOKEN_END func_else_stmt TOKEN_END_IF ';' { yyerrok; }
        | TOKEN_IF '(' error ')' TOKEN_BEGIN func_compound_stmt TOKEN_END TOKEN_END_IF ';' { yyerrok; }
        | TOKEN_IF '(' error ')' func_single_stmt func_else_stmt TOKEN_END_IF ';' { yyerrok; }
        | TOKEN_IF '(' error ')' func_single_stmt TOKEN_END_IF ';' { yyerrok; }
        ;

    func_else_stmt:
          TOKEN_ELSE TOKEN_BEGIN func_compound_stmt TOKEN_END
        | TOKEN_ELSE func_single_stmt
        ;

    func_for_loop:
          TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT TOKEN_BEGIN func_compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: FROM-REPEAT loop\n", current_line); }
        | TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT func_single_stmt { printf("[SYNTAX] Line %d: FROM-REPEAT loop\n", current_line); }
        ;
    
    

    func_def:
          type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN func_compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: Function definition\n", current_line); }
        | type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN func_compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: Function definition\n", current_line); }
        | type TOKEN_FUNCTION TOKEN_ID '(' error ')' decl_list TOKEN_BEGIN func_compound_stmt TOKEN_END ';' { yyerrok; }
        | type TOKEN_FUNCTION TOKEN_ID '(' error ')' TOKEN_BEGIN func_compound_stmt TOKEN_END ';' { yyerrok; }
        ;

    param_decl_list:
          type TOKEN_ID
        | param_decl_list ',' type TOKEN_ID 
        ;

    method_def:
          type TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN func_compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: Method definition\n", current_line); }
        | type TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN func_compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: Method definition\n", current_line); }
        | type TOKEN_ID '(' error ')' decl_list TOKEN_BEGIN func_compound_stmt TOKEN_END ';' { yyerrok; }
        | type TOKEN_ID '(' error ')' TOKEN_BEGIN func_compound_stmt TOKEN_END ';' { yyerrok; }
        ;

    class_def:
          TOKEN_CLASS TOKEN_ID TOKEN_BEGIN class_body TOKEN_END ';' { printf("[SYNTAX] Line %d: Class declaration\n", current_line); }
        | TOKEN_CLASS TOKEN_ID TOKEN_ID TOKEN_BEGIN class_body TOKEN_END ';' { printf("[SYNTAX] Line %d: Class declaration (Tema 24)\n", current_line); }
        ;

    class_body:
          class_body class_member
        | class_member
        ;

    extends_stmt:
          TOKEN_EXTENDS id_list ';' { printf("[SYNTAX] Line %d: Extends statement\n", current_line); }
        | TOKEN_EXTENDS error ';' { yyerrok; }
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
          TOKEN_POUT '(' expr ')' { printf("[SYNTAX] Line %d: POUT statement\n", current_line); }
        | TOKEN_POUT_LOWER '(' expr ')' { printf("[SYNTAX] Line %d: POUT statement\n", current_line); }
        ;

    ret_stmt:
          TOKEN_RET '(' expr ')' { printf("[SYNTAX] Line %d: Return statement (RET)\n", current_line); }
        ;

    %%

    static int yyreport_syntax_error(const yypcontext_t *ctx) {
        yysymbol_kind_t unexp = yypcontext_token(ctx);
        const char *unexp_name = yysymbol_name(unexp);

        if (prev_token == TOKEN_ASSIGN && strcmp(unexp_name, "';'") == 0) {
            fprintf(stderr, "Line %d: Syntax error: Missing expression in assignment\n", current_line);
            return 0;
        }

        if (prev_token == '=' && strcmp(unexp_name, "';'") == 0) {
            fprintf(stderr, "Line %d: Syntax error: Missing expression in attribute assignment\n", current_line);
            return 0;
        }

        if ((prev_token == '<' || prev_token == '>' || prev_token == TOKEN_LESS_EQUAL ||
             prev_token == TOKEN_GREATER_EQUAL || prev_token == TOKEN_EQUAL || prev_token == TOKEN_NOT_EQUAL) &&
            strcmp(unexp_name, "')'") == 0) {
            fprintf(stderr, "Line %d: Syntax error: Incomplete condition (missing operand)\n", current_line);
            return 0;
        }

        if (prev_token == ',' && strcmp(unexp_name, "')'") == 0) {
            fprintf(stderr, "Line %d: Syntax error: Missing parameter or argument after ','\n", current_line);
            return 0;
        }

        if (strcmp(unexp_name, "TOKEN_RET") == 0) {
            fprintf(stderr, "Line %d: Syntax error: Return statement not allowed outside of a function\n", current_line);
            return 0;
        }

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
        return 0;
    }
