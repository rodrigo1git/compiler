/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    TOKEN_ID = 258,                /* TOKEN_ID  */
    TOKEN_STRING = 259,            /* TOKEN_STRING  */
    TOKEN_CONST = 260,             /* TOKEN_CONST  */
    TOKEN_INTEGER = 261,           /* TOKEN_INTEGER  */
    TOKEN_SINGLEF = 262,           /* TOKEN_SINGLEF  */
    TOKEN_BEGIN = 263,             /* TOKEN_BEGIN  */
    TOKEN_END = 264,               /* TOKEN_END  */
    TOKEN_IF = 265,                /* TOKEN_IF  */
    TOKEN_END_IF = 266,            /* TOKEN_END_IF  */
    TOKEN_ELSE = 267,              /* TOKEN_ELSE  */
    TOKEN_FROM = 268,              /* TOKEN_FROM  */
    TOKEN_TO = 269,                /* TOKEN_TO  */
    TOKEN_BY = 270,                /* TOKEN_BY  */
    TOKEN_REPEAT = 271,            /* TOKEN_REPEAT  */
    TOKEN_FUNCTION = 272,          /* TOKEN_FUNCTION  */
    TOKEN_CLASS = 273,             /* TOKEN_CLASS  */
    TOKEN_TOI = 274,               /* TOKEN_TOI  */
    TOKEN_POUT = 275,              /* TOKEN_POUT  */
    TOKEN_POUT_LOWER = 276,        /* TOKEN_POUT_LOWER  */
    TOKEN_RET = 277,               /* TOKEN_RET  */
    TOKEN_COMPTIME = 278,          /* TOKEN_COMPTIME  */
    TOKEN_ASSIGN = 279,            /* TOKEN_ASSIGN  */
    TOKEN_EQUAL = 280,             /* TOKEN_EQUAL  */
    TOKEN_NOT_EQUAL = 281,         /* TOKEN_NOT_EQUAL  */
    TOKEN_LESS_EQUAL = 282,        /* TOKEN_LESS_EQUAL  */
    TOKEN_GREATER_EQUAL = 283      /* TOKEN_GREATER_EQUAL  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 13 "src/grammar.y"

      char* str_val;
      int int_val;
      float float_val;
    

#line 99 "y.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_Y_TAB_H_INCLUDED  */
