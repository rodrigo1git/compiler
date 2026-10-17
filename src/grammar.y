%{
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include "include/symbol_table.h"
  #include "include/scope.h"
  #include "include/codegen.h"

  extern int yylex();
  extern int current_line;
  #include "include/diagnostics.h"
  void yyerror(const char *s);
  map_node_t *add_to_symbol_table(const char *lexeme, const char *category);

  #define SYNERR(location, message) report_syntax_error((location).first_line, (message))
  #define RECERR(location, message) report_recovered_syntax_error((location).first_line, (message))
  #define RECOVER() do { \
      if (is_lexical_recovery() && yychar == YYUNDEF) yyclearin; \
      finish_error_recovery(); \
      yyerrok; \
  } while (0)
  /* Return the consumed ';' to outer recovery instead of skipping the next statement. */
  #define RECOVER_STATEMENT(location, message) do { \
      if (is_lexical_recovery()) SYNERR(location, message); \
      else RECERR(location, message); \
      finish_error_recovery(); \
      yychar = ';'; \
      yylloc = (location); \
      YYERROR; \
  } while (0)
  %}

  %code requires {
    #include "include/symbol_table.h"
    #include "include/scope.h"
    #include "include/codegen.h"
    typedef struct {
      scope_t *scope;
      codegen_function_t *code;
    } function_guard_t;
  }

  /* Token declarations */
  %union {
    map_node_t *symbol_ref;
    scope_t *scope;
    scope_type_t declaration_type;
    scope_names_t *names;
    rpn_t *code;
    size_t jump_target;
    codegen_loop_t loop;
    function_guard_t function;
  }
  %token <symbol_ref> TOKEN_ID TOKEN_CHAIN TOKEN_CONST
  %token TOKEN_INTEGER TOKEN_SINGLEF
  %token TOKEN_BEGIN TOKEN_END TOKEN_IF TOKEN_END_IF TOKEN_ELSE
  %token TOKEN_FROM TOKEN_TO TOKEN_BY TOKEN_REPEAT
  %token TOKEN_FUNCTION TOKEN_CLASS TOKEN_TOI TOKEN_POUT TOKEN_POUT_LOWER TOKEN_RET
  %token TOKEN_COMPTIME TOKEN_EXTENDS
  %type <scope> method_prefix method_header class_header class_missing_code
  %type <function> function_prefix function_header
  %type <declaration_type> type primitive_type
  %type <names> id_list
  %type <code> expr term factor factor_base nonconstant_factor assign_chain cond
  %type <jump_target> if_header if_else_header
  %type <symbol_ref> constant
  %type <symbol_ref> for_identifier for_initial for_final for_step
  %type <loop> for_header
  %destructor { scope_end($$); } <scope>
  %destructor { codegen_function_abort($$.code); scope_end($$.scope); } <function>
  %destructor { scope_names_free($$); } <names>
  %destructor { codegen_free($$); } <code>
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
    | program_name decl_list TOKEN_BEGIN statement TOKEN_END error { RECERR(@6, "Missing ';' after program end"); RECOVER(); }
    | program_name decl_list TOKEN_BEGIN statement YYEOF { SYNERR(@5, "Missing end"); }
    | program_name decl_list assign ';' TOKEN_END ';' { SYNERR(@3, "Missing begin"); }
    | program_name decl_list assign ';' YYEOF { SYNERR(@3, "Missing begin"); }
    ;

program_name:
      TOKEN_ID { scope_program($1, @1.first_line); }
    | error { RECERR(@$, "Missing program name"); RECOVER(); }
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
  type id_list ';' { scope_declare_names($2, $1); trace_printf("[SYNTAX] Line %d: Variable declaration\n", @$.first_line); finish_statement(); }
| TOKEN_COMPTIME type id_list ';' { codegen_unsupported(@1.first_line, "comptime declarations"); scope_declare_names($3, $2); trace_printf("[SYNTAX] Line %d: Comptime variable declaration\n", @$.first_line); finish_statement(); }
| TOKEN_COMPTIME TOKEN_ID ';' { SYNERR(@1, "Missing type in comptime declaration"); finish_statement(); }
| type error ';' {
      if (syntax_error_unexpected() != NULL && strcmp(syntax_error_unexpected(), "TOKEN_ASSIGN") == 0)
          RECERR(@2, "Missing begin");
      else
          RECERR(@2, "Missing identifier in variable declaration");
      RECOVER();
  }
| TOKEN_COMPTIME type error ';' { RECERR(@3, "Missing identifier in comptime declaration"); RECOVER(); }
| TOKEN_COMPTIME error id_list ';' { scope_names_free($3); RECERR(@2, "Missing type in comptime declaration"); RECOVER(); }
| type id_list error ';' { scope_names_free($2); RECERR(@3, "Malformed variable declaration"); RECOVER(); }
| error ';' { report_recovered_syntax_error_with_context(@1.first_line, "Malformed declaration"); RECOVER(); }
;

id_list:
      TOKEN_ID { $$ = scope_names_create($1, @1.first_line); }
    | id_list ',' TOKEN_ID { $$ = scope_names_append($1, $3, @3.first_line); }
    | id_list ',' error { $$ = $1; RECERR(@3, "Missing identifier after ',' in variable declaration"); RECOVER(); }
    | id_list error TOKEN_ID { $$ = scope_names_append($1, $3, @3.first_line); RECERR(@2, "Missing ',' in variable declaration"); RECOVER(); }
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
      assign ';' { finish_statement(); }
    | if_stmt { finish_statement(); }
    | for_loop { finish_statement(); }
    | pout_stmt ';' { finish_statement(); }
    | pout_stmt error { RECERR(@2, "Missing ';' after pout statement"); RECOVER(); }
    | ret_stmt ';' { finish_statement(); }
    | ret_stmt error { RECERR(@2, "Missing ';' after ret statement"); RECOVER(); }
    | TOKEN_POUT_LOWER expr ';' { codegen_free($2); SYNERR(@1, "Missing opening and closing parentheses in pout statement"); finish_statement(); }
    | ret_keyword expr ';' { codegen_free($2); SYNERR(@1, "Missing opening and closing parentheses in ret statement"); finish_statement(); }
    | ret_keyword '(' expr ';' { codegen_free($3); SYNERR(@4, "Missing closing parenthesis in ret statement"); finish_statement(); }
    | TOKEN_ID TOKEN_ASSIGN ';' { scope_resolve($1, SCOPE_VALUE, @1.first_line); SYNERR(@3, "Missing expression on the right side of assignment"); finish_statement(); }
    | attr_ref TOKEN_ASSIGN ';' { SYNERR(@3, "Missing expression on the right side of assignment"); finish_statement(); }
    | attr_ref '=' ';' { SYNERR(@3, "Missing expression on the right side of assignment"); finish_statement(); }
    | error ';' { report_recovered_syntax_error_with_context(@1.first_line, "Malformed statement"); RECOVER(); }
    ;

single_stmt:
      simple_stmt
    ;

type:
      primitive_type
    | TOKEN_ID { $$ = scope_named_type($1, @1.first_line); }
    ;

primitive_type:
      TOKEN_INTEGER { $$ = (scope_type_t){"INTEGER", NULL}; }
    | TOKEN_SINGLEF { $$ = (scope_type_t){"SINGLEF", NULL}; }
    ;

assign:
      TOKEN_ID TOKEN_ASSIGN expr { codegen_assignment(scope_resolve($1, SCOPE_VALUE, @1.first_line), $3, @1.first_line); trace_printf("[SYNTAX] Line %d: Assignment\n", @$.first_line); }
    | attr_ref TOKEN_ASSIGN expr { codegen_free($3); trace_printf("[SYNTAX] Line %d: Attribute Assignment\n", @$.first_line); }
    | attr_ref '=' expr { codegen_free($3); trace_printf("[SYNTAX] Line %d: Attribute Assignment\n", @$.first_line); }
    ;

expr:
      expr '+' term { $$ = codegen_binary($1, $3, RPN_ADD, @2.first_line); }
    | expr '-' term { $$ = codegen_binary($1, $3, RPN_SUBTRACT, @2.first_line); }
    | expr '+' error { codegen_free($1); $$ = NULL; RECERR(@3, "Missing operand after '+'"); RECOVER(); }
    | expr '-' error { codegen_free($1); $$ = NULL; RECERR(@3, "Missing operand after '-'"); RECOVER(); }
    | '+' term { codegen_free($2); $$ = NULL; SYNERR(@1, "Missing left operand before '+'"); }
    | term
    ;

term:
      term '*' factor { $$ = codegen_binary($1, $3, RPN_MULTIPLY, @2.first_line); }
    | term '/' factor { $$ = codegen_binary($1, $3, RPN_DIVIDE, @2.first_line); }
    | term '*' error { codegen_free($1); $$ = NULL; RECERR(@3, "Missing operand after '*'"); RECOVER(); }
    | term '/' error { codegen_free($1); $$ = NULL; RECERR(@3, "Missing operand after '/'"); RECOVER(); }
    | '*' factor { codegen_free($2); $$ = NULL; SYNERR(@1, "Missing left operand before '*'"); }
    | '/' factor { codegen_free($2); $$ = NULL; SYNERR(@1, "Missing left operand before '/'"); }
    | factor
    ;

constant:
      TOKEN_CONST {
          $$ = $1;
          if ($1->provisional) {
              $$ = NULL;
              report_lexical_error(@1.first_line,
                  "Positive integer constant '%s' out of range", $1->key);
          }
      }
    | '-' TOKEN_CONST {
          char *negative = malloc(strlen($2->key) + 2);
          if (negative == NULL) {
              yyerror("Memory exhausted while creating negative constant");
              YYNOMEM;
          }
          sprintf(negative, "-%s", $2->key);
          $$ = add_to_symbol_table(negative, $2->value);
          free(negative);
      }
    | '-' nonconstant_factor { codegen_free($2); $$ = NULL; SYNERR(@1, "Unary '-' is only allowed before a numeric constant"); }
    | '-' error { $$ = NULL; RECERR(@2, "Missing constant after '-'"); RECOVER(); }
    ;

assign_chain:
  TOKEN_ID '=' { $$ = codegen_target(NULL, scope_resolve($1, SCOPE_VALUE, @1.first_line), @1.first_line); }
| assign_chain TOKEN_ID '=' { $$ = codegen_target($1, scope_resolve($2, SCOPE_VALUE, @2.first_line), @2.first_line); }
;

factor:
  assign_chain TOKEN_ID { $$ = codegen_assignment_expression($1, codegen_value(scope_resolve($2, SCOPE_VALUE, @2.first_line), @2.first_line)); }
| assign_chain constant { $$ = codegen_assignment_expression($1, codegen_value($2, @2.first_line)); }
| assign_chain TOKEN_ID '(' arg_list ')' evaluation_order {
      scope_resolve($2, SCOPE_CALLABLE, @2.first_line);
      codegen_free($1); $$ = NULL;
      SYNERR(@2, "Expression assignment with '=' requires an identifier or constant on its right side");
  }
| assign_chain TOKEN_ID '.' TOKEN_ID '(' arg_list ')' evaluation_order {
      scope_resolve($2, SCOPE_VALUE, @2.first_line);
      codegen_free($1); $$ = NULL;
      SYNERR(@2, "Expression assignment with '=' requires an identifier or constant on its right side");
  }
| factor_base
| TOKEN_ID TOKEN_ASSIGN factor_base { scope_resolve($1, SCOPE_VALUE, @1.first_line); codegen_free($3); $$ = NULL; SYNERR(@2, "Use '=' instead of ':=' for assignments in expressions"); }
| TOKEN_ID TOKEN_ASSIGN error { scope_resolve($1, SCOPE_VALUE, @1.first_line); $$ = NULL; RECERR(@2, "Use '=' instead of ':=' for assignments in expressions"); RECOVER(); }
;

factor_base:
  constant { $$ = codegen_value($1, @1.first_line); }
| nonconstant_factor
;

nonconstant_factor:
  call { $$ = NULL; }
| TOKEN_ID { $$ = codegen_value(scope_resolve($1, SCOPE_VALUE, @1.first_line), @1.first_line); }
| TOKEN_TOI '(' expr ')' { $$ = codegen_unary($3, RPN_TO_INTEGER, @1.first_line); }
| TOKEN_TOI '(' ')' { $$ = NULL; SYNERR(@3, "Missing expression in toi conversion"); }
| attr_ref { $$ = NULL; }
;

call:
      call_arguments evaluation_order { codegen_unsupported(@1.first_line, "calls"); }
    | call_arguments { SYNERR(@1, "Missing order for parameter evaluation and assignment"); }
    ;

/* Recover closed arguments locally; stop at ';' when the closing delimiter is missing. */
call_arguments:
      TOKEN_ID '(' arg_list ')' { scope_resolve($1, SCOPE_CALLABLE, @1.first_line); }
    | TOKEN_ID '.' TOKEN_ID '(' arg_list ')' { scope_resolve($1, SCOPE_VALUE, @1.first_line); }
    | TOKEN_ID '.' TOKEN_ID '(' arg_list ',' ')' { scope_resolve($1, SCOPE_VALUE, @1.first_line); SYNERR(@6, "Missing argument after ',' in method call"); }
    | TOKEN_ID '.' TOKEN_ID '(' error ')' { scope_resolve($1, SCOPE_VALUE, @1.first_line); RECERR(@5, "Malformed method arguments"); RECOVER(); }
    | TOKEN_ID '(' arg_list ',' ')' { scope_resolve($1, SCOPE_CALLABLE, @1.first_line); SYNERR(@4, "Missing argument after ',' in function call"); }
    | TOKEN_ID '(' error ')' { scope_resolve($1, SCOPE_CALLABLE, @1.first_line); RECERR(@3, "Malformed function arguments"); RECOVER(); }
    | TOKEN_ID '(' error ';' { scope_resolve($1, SCOPE_CALLABLE, @1.first_line); RECOVER_STATEMENT(@4, "Missing closing parenthesis in function call"); }
    | TOKEN_ID '.' TOKEN_ID '(' error ';' { scope_resolve($1, SCOPE_VALUE, @1.first_line); RECOVER_STATEMENT(@6, "Missing closing parenthesis in method call"); }
    ;

arg_list:
      expr { codegen_free($1); }
    | arg_list ',' expr { codegen_free($3); }
    ;

evaluation_order:
      '[' const_list ']'
    | '[' error ']' { RECERR(@2, "Malformed parameter evaluation order"); RECOVER(); }
    | '[' error ';' { RECOVER_STATEMENT(@3, "Missing closing bracket in parameter evaluation order"); }
    ;

const_list:
      constant
    | const_list ',' constant
    ;

cond:
      assign_chain '>' expr { $$ = NULL; codegen_free($1); codegen_free($3); SYNERR(@2, "Invalid comparator sequence '=' followed by '>'"); }
    | assign_chain '<' expr { $$ = NULL; codegen_free($1); codegen_free($3); SYNERR(@2, "Invalid comparator sequence '=' followed by '<'"); }
    | '<' expr { $$ = NULL; codegen_free($2); SYNERR(@1, "Missing left operand in condition"); }
    | '>' expr { $$ = NULL; codegen_free($2); SYNERR(@1, "Missing left operand in condition"); }
    | TOKEN_GREATER_EQUAL expr { $$ = NULL; codegen_free($2); SYNERR(@1, "Missing left operand in condition"); }
    | TOKEN_LESS_EQUAL expr { $$ = NULL; codegen_free($2); SYNERR(@1, "Missing left operand in condition"); }
    | TOKEN_EQUAL expr { $$ = NULL; codegen_free($2); SYNERR(@1, "Missing left operand in condition"); }
    | TOKEN_NOT_EQUAL expr { $$ = NULL; codegen_free($2); SYNERR(@1, "Missing left operand in condition"); }
    | expr '<' expr { $$ = codegen_binary($1, $3, RPN_LESS, @2.first_line); }
    | expr '>' expr { $$ = codegen_binary($1, $3, RPN_GREATER, @2.first_line); }
    | expr TOKEN_GREATER_EQUAL expr { $$ = codegen_binary($1, $3, RPN_GREATER_EQUAL, @2.first_line); }
    | expr TOKEN_LESS_EQUAL expr { $$ = codegen_binary($1, $3, RPN_LESS_EQUAL, @2.first_line); }
    | expr TOKEN_EQUAL expr { $$ = codegen_binary($1, $3, RPN_EQUAL, @2.first_line); }
    | expr TOKEN_NOT_EQUAL expr { $$ = codegen_binary($1, $3, RPN_NOT_EQUAL, @2.first_line); }
    | expr '<' error { $$ = NULL; codegen_free($1); RECERR(@3, "Missing right operand in condition"); RECOVER(); }
    | expr '>' error { $$ = NULL; codegen_free($1); RECERR(@3, "Missing right operand in condition"); RECOVER(); }
    | expr TOKEN_GREATER_EQUAL error { $$ = NULL; codegen_free($1); RECERR(@3, "Missing right operand in condition"); RECOVER(); }
    | expr TOKEN_LESS_EQUAL error { $$ = NULL; codegen_free($1); RECERR(@3, "Missing right operand in condition"); RECOVER(); }
    | expr TOKEN_EQUAL error { $$ = NULL; codegen_free($1); RECERR(@3, "Missing right operand in condition"); RECOVER(); }
    | expr TOKEN_NOT_EQUAL error { $$ = NULL; codegen_free($1); RECERR(@3, "Missing right operand in condition"); RECOVER(); }
    ;

stmt_block:
      TOKEN_BEGIN compound_stmt TOKEN_END
    | TOKEN_BEGIN compound_stmt YYEOF { SYNERR(@3, "Missing end"); }
    | single_stmt
    ;

if_stmt:
      if_else_header stmt_block TOKEN_END_IF ';' { codegen_if_end($1); trace_printf("[SYNTAX] Line %d: IF statement\n", @1.first_line); }
    | if_header stmt_block TOKEN_END_IF ';' { codegen_if_end($1); trace_printf("[SYNTAX] Line %d: IF statement\n", @1.first_line); }
    | if_header stmt_block error { codegen_if_end($1); RECERR(@3, "Missing end_if"); RECOVER(); }
    | if_else_header stmt_block error { codegen_if_end($1); RECERR(@3, "Missing end_if"); RECOVER(); }
    ;

if_header:
      TOKEN_IF '(' cond ')' { $$ = codegen_if_begin($3, @1.first_line); }
    | TOKEN_IF cond ')' { codegen_free($2); $$ = 0; SYNERR(@1, "Missing opening parenthesis in IF"); }
    | TOKEN_IF '(' cond { codegen_free($3); $$ = 0; SYNERR(@1, "Missing closing parenthesis in IF"); }
    | TOKEN_IF cond { codegen_free($2); $$ = 0; SYNERR(@1, "Missing opening and closing parentheses in IF"); }
    | TOKEN_IF '(' error ')' { $$ = 0; RECERR(@3, "Malformed IF condition"); RECOVER(); }
    | TOKEN_IF '(' error ';' { $$ = 0; RECOVER_STATEMENT(@4, "Missing closing parenthesis in IF condition"); }
    ;

if_else_header:
      if_header stmt_block TOKEN_ELSE { $$ = codegen_if_else($1, @3.first_line); }
    ;

for_loop:
      for_header stmt_block ';' { codegen_for_end($1); trace_printf("[SYNTAX] Line %d: FROM-REPEAT loop\n", @1.first_line); }
    | for_header ';' { codegen_for_end($1); SYNERR(@1, "Missing body in FOR loop"); finish_statement(); }
    | for_header stmt_block error { codegen_for_end($1); RECERR(@3, "Missing ';' after FOR loop"); RECOVER(); }
    ;

for_header:
      TOKEN_FROM for_identifier for_assignment for_initial for_to for_final for_by for_step for_repeat { $$ = codegen_for_begin($2, $4, $6, $8, @1.first_line); }
    | TOKEN_ID TOKEN_ASSIGN constant TOKEN_TO for_final for_by for_step for_repeat { $$ = (codegen_loop_t){0}; scope_resolve($1, SCOPE_VALUE, @1.first_line); SYNERR(@1, "Missing 'from' in FOR loop"); }
    ;

/* Recover each component before parsing the next component or the body.
   At end of input, stop instead of retrying a component that cannot be supplied. */
for_identifier:
      TOKEN_ID { $$ = scope_resolve($1, SCOPE_VALUE, @1.first_line); }
    | error { $$ = NULL; RECERR(@1, "Missing identifier in FOR loop"); if (yychar == YYEOF) YYABORT; RECOVER(); }
    ;

for_assignment:
      TOKEN_ASSIGN
    | error { RECERR(@1, "Missing ':=' in FOR loop"); if (yychar == YYEOF) YYABORT; RECOVER(); }
    ;

for_initial:
      constant
    | error { $$ = NULL; RECERR(@1, "Missing constant after 'from' in FOR loop"); if (yychar == YYEOF) YYABORT; RECOVER(); }
    ;

for_to:
      TOKEN_TO
    | error { RECERR(@1, "Missing 'to' in FOR loop"); if (yychar == YYEOF) YYABORT; RECOVER(); }
    ;

for_final:
      constant
    | error { $$ = NULL; RECERR(@1, "Missing constant after 'to' in FOR loop"); if (yychar == YYEOF) YYABORT; RECOVER(); }
    ;

for_by:
      TOKEN_BY
    | error { RECERR(@1, "Missing 'by' in FOR loop"); if (yychar == YYEOF) YYABORT; RECOVER(); }
    ;

for_step:
      constant
    | error { $$ = NULL; RECERR(@1, "Missing constant after 'by' in FOR loop"); if (yychar == YYEOF) YYABORT; RECOVER(); }
    ;

for_repeat:
      TOKEN_REPEAT
    | %empty { SYNERR(@$, "Missing 'repeat' in FOR loop"); }
    ;

func_def:
      function_header decl_list TOKEN_BEGIN compound_stmt TOKEN_END ';' { codegen_function_end($1.code); scope_end($1.scope); trace_printf("[SYNTAX] Line %d: Function definition\n", @1.first_line); }
    | function_header TOKEN_BEGIN compound_stmt TOKEN_END ';' { codegen_function_end($1.code); scope_end($1.scope); trace_printf("[SYNTAX] Line %d: Function definition\n", @1.first_line); }
    | function_prefix error ';' { codegen_function_abort($1.code); scope_end($1.scope); RECERR(@2, "Malformed function header"); RECOVER(); }
    | function_header decl_list TOKEN_BEGIN compound_stmt YYEOF { codegen_function_abort($1.code); scope_end($1.scope); SYNERR(@4, "Missing end of function"); YYABORT; }
    | function_header TOKEN_BEGIN compound_stmt YYEOF { codegen_function_abort($1.code); scope_end($1.scope); SYNERR(@3, "Missing end of function"); YYABORT; }
    ;

/* Open the scope before parameters and local declarations are recognized. */
function_prefix:
      type TOKEN_FUNCTION TOKEN_ID '(' { $$.scope = scope_begin($3, SYMBOL_FUNCTION, $1, @3.first_line); $$.code = codegen_function_begin(scope_symbol($$.scope), @3.first_line); }
    | type TOKEN_FUNCTION error '(' { RECERR(@3, "Missing function name"); RECOVER(); $$.scope = scope_begin(NULL, SYMBOL_FUNCTION, $1, @3.first_line); $$.code = NULL; }
    ;

function_header:
      function_prefix param_decl_list ')' { $$ = $1; }
    | function_prefix error ')' { $$ = $1; RECERR(@2, "Malformed formal parameter list"); RECOVER(); }
    ;

param_decl_list:
      type TOKEN_ID { scope_parameter($2, $1, @2.first_line); }
    | param_decl_list ',' type TOKEN_ID { scope_parameter($4, $3, @4.first_line); }
    | primitive_type error { RECERR(@2, "Missing formal parameter name"); RECOVER(); }
    | error TOKEN_ID { RECERR(@1, "Missing formal parameter type"); RECOVER(); }
    | param_decl_list ',' primitive_type error { RECERR(@4, "Missing formal parameter name"); RECOVER(); }
    | param_decl_list ',' error TOKEN_ID { RECERR(@3, "Missing formal parameter type"); RECOVER(); }
    ;

method_def:
      method_header decl_list TOKEN_BEGIN compound_stmt TOKEN_END ';' { scope_end($1); trace_printf("[SYNTAX] Line %d: Method definition\n", @1.first_line); }
    | method_header TOKEN_BEGIN compound_stmt TOKEN_END ';' { scope_end($1); trace_printf("[SYNTAX] Line %d: Method definition\n", @1.first_line); }
    | method_prefix error ';' { scope_end($1); RECERR(@2, "Malformed method header"); RECOVER(); }
    ;

method_prefix:
      type TOKEN_ID '(' { codegen_unsupported(@2.first_line, "method definitions"); $$ = scope_begin($2, SYMBOL_METHOD, $1, @2.first_line); }
    ;

method_header:
      method_prefix param_decl_list ')' { $$ = $1; }
    | method_prefix error ')' { $$ = $1; RECERR(@2, "Malformed formal parameter list"); RECOVER(); }
    ;

class_header:
      TOKEN_CLASS TOKEN_ID TOKEN_ID { codegen_unsupported(@1.first_line, "class declarations"); $$ = scope_begin($2, SYMBOL_CLASS, (scope_type_t){NULL, NULL}, @2.first_line); }
    ;

class_missing_code:
      TOKEN_CLASS TOKEN_ID TOKEN_BEGIN { SYNERR(@1, "Missing class code"); $$ = scope_begin($2, SYMBOL_CLASS, (scope_type_t){NULL, NULL}, @2.first_line); }
    ;

class_def:
      class_header TOKEN_BEGIN class_body TOKEN_END ';' { scope_end($1); trace_printf("[SYNTAX] Line %d: Class declaration (Topic 24)\n", @1.first_line); }
    | class_missing_code class_body TOKEN_END ';' { scope_end($1); finish_statement(); }
    | class_header error TOKEN_END ';' { scope_end($1); RECERR(@2, "Missing 'begin' in class declaration"); RECOVER(); }
    | class_header TOKEN_BEGIN error TOKEN_END ';' { scope_end($1); RECERR(@3, "Malformed class body"); RECOVER(); }
    | TOKEN_CLASS TOKEN_ID error TOKEN_END ';' { RECERR(@3, "Malformed class declaration"); RECOVER(); }
    ;

class_body:
      class_body class_member
    | class_member
    ;

extends_stmt:
      TOKEN_EXTENDS extends_id_list ';' { trace_printf("[SYNTAX] Line %d: Extends statement\n", @$.first_line); }
    | TOKEN_EXTENDS extends_id_list ',' error ';' { RECERR(@4, "Missing class name after ',' in extends list"); RECOVER(); }
    | TOKEN_EXTENDS error ';' { RECERR(@2, "Missing class list after extends"); RECOVER(); }
    ;

extends_id_list:
      TOKEN_ID { scope_resolve($1, SCOPE_CLASS, @1.first_line); }
    | extends_id_list ',' TOKEN_ID { scope_resolve($3, SCOPE_CLASS, @3.first_line); }
    ;

class_member:
      var_decl
    | method_def
    | extends_stmt
    ;

attr_ref:
      TOKEN_ID '[' constant ']' { scope_resolve($1, SCOPE_VALUE, @1.first_line); codegen_unsupported(@1.first_line, "attribute references"); }
    | TOKEN_ID '[' TOKEN_ID ']' { scope_resolve($1, SCOPE_VALUE, @1.first_line); scope_resolve($3, SCOPE_VALUE, @3.first_line); codegen_unsupported(@1.first_line, "attribute references"); }
    ;

pout_stmt:
      TOKEN_POUT_LOWER '(' expr ')' { codegen_output($3, @1.first_line); trace_printf("[SYNTAX] Line %d: POUT statement\n", @$.first_line); }
    | TOKEN_POUT_LOWER '(' TOKEN_CHAIN ')' { codegen_output(codegen_value($3, @3.first_line), @1.first_line); trace_printf("[SYNTAX] Line %d: POUT statement\n", @$.first_line); }
    | TOKEN_POUT_LOWER expr ')' { codegen_free($2); SYNERR(@1, "Missing opening parenthesis in pout statement"); }
    | TOKEN_POUT_LOWER '(' error ')' { RECERR(@3, "Missing argument in pout"); RECOVER(); }
    | TOKEN_POUT_LOWER '(' error ';' { RECOVER_STATEMENT(@4, "Missing closing parenthesis in pout statement"); }
    ;

ret_keyword:
      TOKEN_RET {
          if (!scope_in_function())
              SYNERR(@1, "Forbidden return (Outside function context)");
      }
    ;

ret_stmt:
      ret_keyword expr ')' { codegen_free($2); SYNERR(@1, "Missing opening parenthesis in ret statement"); }
    | ret_keyword '(' ')' { SYNERR(@3, "Missing expression in ret statement"); }
    | ret_keyword '(' expr ')' {
          if (scope_in_function()) {
              codegen_return($3, @1.first_line);
              trace_printf("[SYNTAX] Line %d: Return statement (RET)\n", @1.first_line);
          } else codegen_free($3);
      }
    | ret_keyword '(' expr error { codegen_free($3); RECERR(@4, "Missing closing parenthesis in ret statement"); RECOVER(); }
    ;

%%

static const char *friendly_symbol_name(yysymbol_kind_t symbol);

static int yyreport_syntax_error(const yypcontext_t *ctx) {
    enum { EXPECTED_DISPLAY_LIMIT = 3 };
    yysymbol_kind_t expected[YYNTOKENS];
    yysymbol_kind_t unexpected = yypcontext_token(ctx);
    YYLTYPE *location = yypcontext_location(ctx);
    int expected_count;
    int shown_count;
    size_t used;
    int expects_operator = 0;
    int expects_arithmetic_operand = 0;

    const char *recovered_unexpected_name = yysymbol_name(unexpected);
    int recovered_error_context_line;
    char recovered_error_context[256];
    recovered_error_context_line = location != NULL ? location->first_line : current_line;
    recovered_error_context[0] = '\0';

    expected_count = yypcontext_expected_tokens(ctx, expected, YYNTOKENS);
    if (expected_count < 0) {
        return expected_count;
    }
    for (int i = 0; i < expected_count; i++) {
        const char *name = yysymbol_name(expected[i]);
        if (strcmp(name, "'+'") == 0 || strcmp(name, "'-'") == 0 ||
            strcmp(name, "'*'") == 0 || strcmp(name, "'/'") == 0)
            expects_operator = 1;
        if (expected[i] == YYSYMBOL_TOKEN_TOI)
            expects_arithmetic_operand = 1;
    }
    if (unexpected != YYSYMBOL_YYEMPTY) {
        const char *detail = "";
        if (unexpected == YYSYMBOL_TOKEN_CONST && expects_operator)
            detail = "Missing operator in expression; ";
        else if (unexpected == YYSYMBOL_TOKEN_CHAIN && expects_arithmetic_operand)
            detail = "String literal cannot be used as an arithmetic operand; ";
        snprintf(recovered_error_context, sizeof(recovered_error_context),
                  "%sunexpected %s", detail, friendly_symbol_name(unexpected));
    }
    if (expected_count > 0) {
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
    begin_syntax_error(recovered_error_context_line, recovered_unexpected_name,
                        recovered_error_context);
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
