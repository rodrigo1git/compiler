%{
#include <stdio.h>
#include <stdlib.h>

extern int yylex();
void yyerror(const char *s);
%}

/* 
 * Declaración de Tokens
 */
%token TOKEN_ID TOKEN_CONST TOKEN_STRING
%token TOKEN_INTEGER TOKEN_SINGLEF
%token TOKEN_BEGIN TOKEN_END TOKEN_IF TOKEN_END_IF TOKEN_ELSE
%token TOKEN_FROM TOKEN_TO TOKEN_BY TOKEN_REPEAT
%token TOKEN_FUNCTION TOKEN_CLASS TOKEN_TOI TOKEN_POUT TOKEN_POUT_LOWER TOKEN_RET
%token TOKEN_COMPTIME

/* Operadores relacionales y de asignación */
%token TOKEN_ASSIGN          /* := */
%token TOKEN_EQUAL           /* == */
%token TOKEN_NOT_EQUAL       /* != */
%token TOKEN_LESS_EQUAL      /* <= */
%token TOKEN_GREATER_EQUAL   /* >= */

%start statements

%%

/* ---------------------------------------------------------
   Reglas Gramaticales
   --------------------------------------------------------- */

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
      type id_list ';'
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
      TOKEN_ID TOKEN_ASSIGN expr
    | TOKEN_ID TOKEN_ASSIGN call
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

factor:
      TOKEN_ID '=' factor
    | TOKEN_ID
    | TOKEN_CONST
    | TOKEN_STRING
    ;

call:
      TOKEN_ID '(' arg_list ')' '[' const_list ']'
    | TOKEN_ID '(' arg_list ')'
    ;

arg_list:
      expr
    | expr ',' arg_list
    ;

const_list:
      TOKEN_CONST
    | TOKEN_CONST ',' const_list
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
    | TOKEN_IF '(' cond ')' single_stmt TOKEN_END_IF
    ;

else_stmt:
      TOKEN_ELSE TOKEN_BEGIN compound_stmt TOKEN_END
    | TOKEN_ELSE single_stmt
    ;

for_loop:
      TOKEN_FROM TOKEN_ID TOKEN_ASSIGN TOKEN_CONST TOKEN_TO TOKEN_CONST TOKEN_BY TOKEN_CONST TOKEN_REPEAT compound_stmt ';'
    ;

func_def:
      type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' decl_list TOKEN_BEGIN compound_stmt TOKEN_END ';'
    | type TOKEN_FUNCTION TOKEN_ID '(' param_decl_list ')' TOKEN_BEGIN compound_stmt TOKEN_END ';'
    ;

param_decl_list:
      type TOKEN_ID
    | type TOKEN_ID ',' param_decl_list
    ;

class_def:
      TOKEN_CLASS id_list TOKEN_BEGIN assign_list func_list TOKEN_END ';'
    ;

assign_list:
      assign_list assign ';'
    | assign ';'
    ;

func_list:
      func_list func_def
    | func_def
    ;

attr_access:
      TOKEN_ID '[' TOKEN_CONST ']' '=' expr
    | TOKEN_ID '[' TOKEN_ID ']' '=' expr
    ;

toi_call:
      TOKEN_TOI '(' expr ')'
    ;

pout_stmt:
      TOKEN_POUT '(' expr ')'
    | TOKEN_POUT_LOWER '(' expr ')'
    ;

ret_stmt:
      TOKEN_RET expr
    | TOKEN_RET
    ;

%%