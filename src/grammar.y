 %{
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    
    extern int yylex();
    extern int current_line;
    void yyerror(const char *s);
    void add_to_symbol_table(const char* lexeme_buffer, const char* tipo);
    %}
    
    /* Token declarations */
    %union {
      char* str_val;
      int int_val;
      float float_val;
    }
    %token TOKEN_ID TOKEN_STRING
    %token <str_val> TOKEN_CONST
    %token TOKEN_INTEGER TOKEN_SINGLEF
    %token TOKEN_BEGIN TOKEN_END TOKEN_IF TOKEN_END_IF TOKEN_ELSE
    %token TOKEN_FROM TOKEN_TO TOKEN_BY TOKEN_REPEAT
    %token TOKEN_FUNCTION TOKEN_CLASS TOKEN_TOI TOKEN_POUT TOKEN_POUT_LOWER TOKEN_RET
    %token TOKEN_COMPTIME
    
    /* Relational and assignment operators */
    %token TOKEN_ASSIGN          /* := */
    %token TOKEN_EQUAL           /* == */
    %token TOKEN_NOT_EQUAL       /* != */
    %token TOKEN_LESS_EQUAL      /* <= */
    %token TOKEN_GREATER_EQUAL   /* >= */
    
    %define parse.error verbose

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
        | error ';' { yyerrok; }
        ;
    
    id_list:
          TOKEN_ID
        | TOKEN_ID ',' id_list
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
        | ret_stmt ';'
        | toi_call ';'
        | error ';' { yyerrok; }
        ;
    
    single_stmt:
          assign ';'
        | if_stmt
        | attr_access ';'
        | pout_stmt ';'
        | ret_stmt ';'
        | toi_call ';'
        | error ';' { yyerrok; }
        ;
    
    type:
          TOKEN_INTEGER
        | TOKEN_SINGLEF
        ;
    
    assign:
          TOKEN_ID TOKEN_ASSIGN expr { printf("[SYNTAX] Line %d: Assignment\n", current_line); }
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
              long val = atol($1);
              if (val > 32767) {
                  yyerror("Semantic error: Positive constant out of range");
              }
          }
        | '-' TOKEN_CONST {
              char neg_str[100];
              sprintf(neg_str, "-%s", $2);
              long val = atol(neg_str);
              if (val < -32768) {
                  yyerror("Semantic error: Negative constant out of range");
              } else {
                  add_to_symbol_table(neg_str, "INTEGER");
              }
          }
        ;
    
    factor:
          TOKEN_ID '=' factor
        | call
        | TOKEN_ID
        | constant
        | TOKEN_STRING
        ;
    
    call:
          TOKEN_ID '(' arg_list ')' '[' const_list ']'
        ;
    
    arg_list:
          expr
        | expr ',' arg_list
        ;
    
    const_list:
          constant
        | constant ',' const_list
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
          TOKEN_IF '(' cond ')' TOKEN_BEGIN compound_stmt TOKEN_END else_stmt TOKEN_END_IF
        | TOKEN_IF '(' cond ')' single_stmt else_stmt TOKEN_END_IF
        | TOKEN_IF '(' cond ')' single_stmt TOKEN_END_IF { printf("[SYNTAX] Line %d: IF statement\n", current_line); }
        ;

    else_stmt:
          TOKEN_ELSE TOKEN_BEGIN compound_stmt TOKEN_END
        | TOKEN_ELSE single_stmt
        ;

    for_loop:
          TOKEN_FROM TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO constant TOKEN_BY constant TOKEN_REPEAT compound_stmt ';' { printf("[SYNTAX] Line %d: FROM-REPEAT loop\n", current_line); }
        ;

    func_def:
          type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN compound_stmt TOKEN_END ';'
        | type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN compound_stmt TOKEN_END ';' { printf("[SYNTAX] Line %d: Function definition\n", current_line); }
        ;

    param_decl_list:
          type TOKEN_ID
        | type TOKEN_ID ',' param_decl_list
        ;

    class_def:
          TOKEN_CLASS TOKEN_ID TOKEN_ID TOKEN_BEGIN class_body TOKEN_END ';' { printf("[SYNTAX] Line %d: Class declaration\n", current_line); }
        ;

    class_body:
          class_body class_member
        | class_member
        ;

    class_member:
          var_decl
        | func_def
        | assign ';'
        ;

    attr_access:
          TOKEN_ID '[' constant ']' '=' expr
        | TOKEN_ID '[' TOKEN_ID ']' '=' expr
        ;

    toi_call:
          TOKEN_TOI '(' expr ')' { printf("[SYNTAX] Line %d: TOI call\n", current_line); }
        ;

    pout_stmt:
          TOKEN_POUT '(' expr ')' { printf("[SYNTAX] Line %d: POUT statement\n", current_line); }
        | TOKEN_POUT_LOWER '(' expr ')' { printf("[SYNTAX] Line %d: POUT statement\n", current_line); }
        ;

    ret_stmt:
          TOKEN_RET expr { printf("[SYNTAX] Line %d: Return statement (RET)\n", current_line); }
        | TOKEN_RET { printf("[SYNTAX] Line %d: Return statement (RET)\n", current_line); }
        ;

    %%
