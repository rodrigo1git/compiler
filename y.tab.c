/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "src/grammar.y"

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    
    extern int yylex();
    void yyerror(const char *s);
    void add_to_symbol_table(const char* lexema, const char* tipo);
    

#line 82 "y.tab.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
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
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define TOKEN_ID 258
#define TOKEN_STRING 259
#define TOKEN_CONST 260
#define TOKEN_INTEGER 261
#define TOKEN_SINGLEF 262
#define TOKEN_BEGIN 263
#define TOKEN_END 264
#define TOKEN_IF 265
#define TOKEN_END_IF 266
#define TOKEN_ELSE 267
#define TOKEN_FROM 268
#define TOKEN_TO 269
#define TOKEN_BY 270
#define TOKEN_REPEAT 271
#define TOKEN_FUNCTION 272
#define TOKEN_CLASS 273
#define TOKEN_TOI 274
#define TOKEN_POUT 275
#define TOKEN_POUT_LOWER 276
#define TOKEN_RET 277
#define TOKEN_COMPTIME 278
#define TOKEN_ASSIGN 279
#define TOKEN_EQUAL 280
#define TOKEN_NOT_EQUAL 281
#define TOKEN_LESS_EQUAL 282
#define TOKEN_GREATER_EQUAL 283

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 14 "src/grammar.y"

      char* str_val;
      int int_val;
      float float_val;
    

#line 198 "y.tab.c"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_Y_TAB_H_INCLUDED  */
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_TOKEN_ID = 3,                   /* TOKEN_ID  */
  YYSYMBOL_TOKEN_STRING = 4,               /* TOKEN_STRING  */
  YYSYMBOL_TOKEN_CONST = 5,                /* TOKEN_CONST  */
  YYSYMBOL_TOKEN_INTEGER = 6,              /* TOKEN_INTEGER  */
  YYSYMBOL_TOKEN_SINGLEF = 7,              /* TOKEN_SINGLEF  */
  YYSYMBOL_TOKEN_BEGIN = 8,                /* TOKEN_BEGIN  */
  YYSYMBOL_TOKEN_END = 9,                  /* TOKEN_END  */
  YYSYMBOL_TOKEN_IF = 10,                  /* TOKEN_IF  */
  YYSYMBOL_TOKEN_END_IF = 11,              /* TOKEN_END_IF  */
  YYSYMBOL_TOKEN_ELSE = 12,                /* TOKEN_ELSE  */
  YYSYMBOL_TOKEN_FROM = 13,                /* TOKEN_FROM  */
  YYSYMBOL_TOKEN_TO = 14,                  /* TOKEN_TO  */
  YYSYMBOL_TOKEN_BY = 15,                  /* TOKEN_BY  */
  YYSYMBOL_TOKEN_REPEAT = 16,              /* TOKEN_REPEAT  */
  YYSYMBOL_TOKEN_FUNCTION = 17,            /* TOKEN_FUNCTION  */
  YYSYMBOL_TOKEN_CLASS = 18,               /* TOKEN_CLASS  */
  YYSYMBOL_TOKEN_TOI = 19,                 /* TOKEN_TOI  */
  YYSYMBOL_TOKEN_POUT = 20,                /* TOKEN_POUT  */
  YYSYMBOL_TOKEN_POUT_LOWER = 21,          /* TOKEN_POUT_LOWER  */
  YYSYMBOL_TOKEN_RET = 22,                 /* TOKEN_RET  */
  YYSYMBOL_TOKEN_COMPTIME = 23,            /* TOKEN_COMPTIME  */
  YYSYMBOL_TOKEN_ASSIGN = 24,              /* TOKEN_ASSIGN  */
  YYSYMBOL_TOKEN_EQUAL = 25,               /* TOKEN_EQUAL  */
  YYSYMBOL_TOKEN_NOT_EQUAL = 26,           /* TOKEN_NOT_EQUAL  */
  YYSYMBOL_TOKEN_LESS_EQUAL = 27,          /* TOKEN_LESS_EQUAL  */
  YYSYMBOL_TOKEN_GREATER_EQUAL = 28,       /* TOKEN_GREATER_EQUAL  */
  YYSYMBOL_29_ = 29,                       /* ';'  */
  YYSYMBOL_30_ = 30,                       /* ','  */
  YYSYMBOL_31_ = 31,                       /* '+'  */
  YYSYMBOL_32_ = 32,                       /* '-'  */
  YYSYMBOL_33_ = 33,                       /* '*'  */
  YYSYMBOL_34_ = 34,                       /* '/'  */
  YYSYMBOL_35_ = 35,                       /* '='  */
  YYSYMBOL_36_ = 36,                       /* '('  */
  YYSYMBOL_37_ = 37,                       /* ')'  */
  YYSYMBOL_38_ = 38,                       /* '['  */
  YYSYMBOL_39_ = 39,                       /* ']'  */
  YYSYMBOL_40_ = 40,                       /* '<'  */
  YYSYMBOL_41_ = 41,                       /* '>'  */
  YYSYMBOL_YYACCEPT = 42,                  /* $accept  */
  YYSYMBOL_statements = 43,                /* statements  */
  YYSYMBOL_program_name = 44,              /* program_name  */
  YYSYMBOL_decl_list = 45,                 /* decl_list  */
  YYSYMBOL_var_decl = 46,                  /* var_decl  */
  YYSYMBOL_id_list = 47,                   /* id_list  */
  YYSYMBOL_statement = 48,                 /* statement  */
  YYSYMBOL_compound_stmt = 49,             /* compound_stmt  */
  YYSYMBOL_simple_stmt = 50,               /* simple_stmt  */
  YYSYMBOL_single_stmt = 51,               /* single_stmt  */
  YYSYMBOL_type = 52,                      /* type  */
  YYSYMBOL_assign = 53,                    /* assign  */
  YYSYMBOL_expr = 54,                      /* expr  */
  YYSYMBOL_term = 55,                      /* term  */
  YYSYMBOL_constant = 56,                  /* constant  */
  YYSYMBOL_factor = 57,                    /* factor  */
  YYSYMBOL_call = 58,                      /* call  */
  YYSYMBOL_arg_list = 59,                  /* arg_list  */
  YYSYMBOL_const_list = 60,                /* const_list  */
  YYSYMBOL_cond = 61,                      /* cond  */
  YYSYMBOL_if_stmt = 62,                   /* if_stmt  */
  YYSYMBOL_else_stmt = 63,                 /* else_stmt  */
  YYSYMBOL_for_loop = 64,                  /* for_loop  */
  YYSYMBOL_func_def = 65,                  /* func_def  */
  YYSYMBOL_param_decl_list = 66,           /* param_decl_list  */
  YYSYMBOL_class_def = 67,                 /* class_def  */
  YYSYMBOL_class_body = 68,                /* class_body  */
  YYSYMBOL_class_member = 69,              /* class_member  */
  YYSYMBOL_attr_access = 70,               /* attr_access  */
  YYSYMBOL_toi_call = 71,                  /* toi_call  */
  YYSYMBOL_pout_stmt = 72,                 /* pout_stmt  */
  YYSYMBOL_ret_stmt = 73                   /* ret_stmt  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  4
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   273

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  42
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  32
/* YYNRULES -- Number of rules.  */
#define YYNRULES  81
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  187

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   283


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      36,    37,    33,    31,    30,    32,     2,    34,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    29,
      40,    35,    41,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    38,     2,    39,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint8 yyrline[] =
{
       0,    43,    43,    47,    51,    52,    53,    54,    55,    56,
      60,    61,    65,    66,    70,    74,    75,    79,    80,    81,
      82,    83,    84,    85,    86,    90,    91,    92,    93,    94,
      95,    96,   100,   101,   105,   109,   110,   111,   115,   116,
     117,   121,   127,   140,   141,   142,   143,   144,   148,   152,
     153,   157,   158,   162,   163,   164,   165,   166,   167,   171,
     172,   173,   177,   178,   182,   186,   187,   191,   192,   196,
     200,   201,   205,   206,   207,   211,   212,   216,   220,   221,
     225,   226
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "TOKEN_ID",
  "TOKEN_STRING", "TOKEN_CONST", "TOKEN_INTEGER", "TOKEN_SINGLEF",
  "TOKEN_BEGIN", "TOKEN_END", "TOKEN_IF", "TOKEN_END_IF", "TOKEN_ELSE",
  "TOKEN_FROM", "TOKEN_TO", "TOKEN_BY", "TOKEN_REPEAT", "TOKEN_FUNCTION",
  "TOKEN_CLASS", "TOKEN_TOI", "TOKEN_POUT", "TOKEN_POUT_LOWER",
  "TOKEN_RET", "TOKEN_COMPTIME", "TOKEN_ASSIGN", "TOKEN_EQUAL",
  "TOKEN_NOT_EQUAL", "TOKEN_LESS_EQUAL", "TOKEN_GREATER_EQUAL", "';'",
  "','", "'+'", "'-'", "'*'", "'/'", "'='", "'('", "')'", "'['", "']'",
  "'<'", "'>'", "$accept", "statements", "program_name", "decl_list",
  "var_decl", "id_list", "statement", "compound_stmt", "simple_stmt",
  "single_stmt", "type", "assign", "expr", "term", "constant", "factor",
  "call", "arg_list", "const_list", "cond", "if_stmt", "else_stmt",
  "for_loop", "func_def", "param_decl_list", "class_def", "class_body",
  "class_member", "attr_access", "toi_call", "pout_stmt", "ret_stmt", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-125)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-15)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      18,  -125,    23,    29,  -125,    -4,  -125,  -125,    26,   100,
    -125,    54,  -125,  -125,  -125,    64,   195,  -125,  -125,  -125,
      42,    73,    66,    92,    86,    35,    87,   123,    96,   107,
     109,    11,   127,    83,  -125,   117,  -125,  -125,   119,   124,
     128,   129,   161,   134,  -125,    84,  -125,    11,    14,    11,
     149,    11,    11,    11,    39,  -125,  -125,   167,    17,    31,
    -125,  -125,  -125,   162,  -125,  -125,  -125,  -125,  -125,  -125,
    -125,   104,   151,  -125,   170,  -125,   121,  -125,    17,   145,
     158,   209,   164,    13,    57,   148,   207,    11,    11,  -125,
      11,    11,    11,    11,  -125,   200,   175,  -125,   181,  -125,
     192,   193,    11,    11,    11,    11,    11,    11,   199,   215,
    -125,  -125,  -125,  -125,     1,   208,    31,    31,  -125,  -125,
     216,   113,  -125,    11,    11,    17,    17,    17,    17,    17,
      17,   201,   195,   101,   222,  -125,   223,   224,   226,   227,
      13,    11,   219,   104,   195,   225,    17,    17,  -125,   141,
    -125,   203,   243,  -125,  -125,  -125,  -125,  -125,   244,  -125,
      13,  -125,   146,   195,   246,   195,  -125,  -125,    13,   230,
     228,   232,   168,   251,   173,   247,    13,  -125,  -125,   235,
    -125,  -125,   195,  -125,  -125,    41,  -125
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     3,     0,     0,     1,     0,    32,    33,     0,     0,
       7,     0,     9,     8,    11,     0,     0,     4,     6,     5,
      12,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    81,     0,     0,    16,     0,    18,    19,     0,     0,
       0,     0,     0,     0,    10,     0,    24,     0,     0,     0,
       0,     0,     0,     0,    45,    47,    41,     0,    80,    37,
      46,    40,    44,     0,    15,    17,    20,    23,    21,    22,
      13,     0,     0,    72,     0,    73,     0,    71,    34,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    42,
       0,     0,     0,     0,     2,     0,     0,    74,     0,    70,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      77,    78,    79,    43,    49,     0,    35,    36,    38,    39,
      67,     0,    69,     0,     0,    57,    58,    56,    55,    53,
      54,     0,     0,     0,     0,    26,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    76,    75,    31,     0,
      61,     0,     0,    25,    27,    30,    28,    29,     0,    50,
       0,    68,     0,     0,     0,     0,    63,    60,     0,    51,
       0,     0,     0,     0,     0,     0,     0,    48,    66,     0,
      59,    62,     0,    52,    65,     0,    64
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -125,  -125,  -125,   144,    -8,   229,  -125,  -124,   -33,   115,
     -60,   -42,   -25,    44,   -43,   155,  -125,   131,    93,  -125,
    -101,   106,  -125,    -7,   125,    -5,  -125,   197,   -99,   -98,
     -96,   -95
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,     2,     3,     9,    10,    22,    32,    33,    34,   133,
      11,    35,   114,    59,    60,    61,    62,   115,   170,    82,
      36,   152,    37,    12,    96,    13,    76,    77,    38,    39,
      40,    41
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      64,    17,    18,    74,    19,    80,    58,   135,   149,   136,
     137,    95,   138,   139,    54,    55,    56,    79,    56,    56,
     162,     1,    78,     4,    81,    14,    84,    85,    86,    15,
       5,   141,    90,    91,    74,     6,     7,    73,    75,   172,
     109,   174,    24,    57,    25,    57,    57,     8,    90,    91,
     135,    26,   136,   137,    27,   138,   139,    20,   185,    47,
      28,    29,    30,    31,    92,    93,   134,    23,    73,    75,
     186,    21,    42,    48,    87,    88,    43,   125,   126,   127,
     128,   129,   130,    95,    24,     5,    25,    72,    90,    91,
       6,     7,   -14,    26,   110,    44,    27,   158,   146,   147,
      45,     5,    28,    29,    30,    31,     6,     7,    16,   134,
       6,     7,   150,   151,     5,    46,    64,   169,     8,     6,
       7,   144,     5,    49,    72,   175,    50,     6,     7,    64,
      98,     8,    51,   169,   116,   117,    63,    17,    18,    64,
      19,    64,    24,    52,    25,    53,    65,    24,    66,    25,
     164,    26,    64,    67,    27,   171,    26,    68,    69,    27,
      28,    29,    30,    31,    20,    28,    29,    30,    31,    24,
      71,    25,    89,    83,    24,    47,    25,   179,    26,    90,
      91,    27,   181,    26,   100,   111,    27,    28,    29,    30,
      31,    94,    28,    29,    30,    31,    24,   101,    25,    97,
     131,   108,    25,   120,   131,    26,    25,   132,    27,    26,
     122,   165,   121,    26,    28,    29,    30,    31,    28,    29,
      30,    31,    28,    29,    30,    31,     5,   123,   124,   140,
     148,     6,     7,   163,   102,   103,   104,   105,    90,    91,
      90,    91,   113,     8,   112,   142,   143,   118,   119,   106,
     107,   153,   154,   155,   167,   156,   157,   160,   151,   168,
     176,   178,   180,   182,   184,   145,   166,   177,   161,   183,
     173,    70,   159,    99
};

static const yytype_uint8 yycheck[] =
{
      33,     9,     9,    45,     9,    48,    31,   108,   132,   108,
     108,    71,   108,   108,     3,     4,     5,     3,     5,     5,
     144,     3,    47,     0,    49,    29,    51,    52,    53,     3,
       1,    30,    31,    32,    76,     6,     7,    45,    45,   163,
      83,   165,     1,    32,     3,    32,    32,    18,    31,    32,
     151,    10,   151,   151,    13,   151,   151,     3,   182,    24,
      19,    20,    21,    22,    33,    34,   108,     3,    76,    76,
      29,    17,    30,    38,    35,    36,     3,   102,   103,   104,
     105,   106,   107,   143,     1,     1,     3,     3,    31,    32,
       6,     7,     9,    10,    37,    29,    13,   140,   123,   124,
       8,     1,    19,    20,    21,    22,     6,     7,     8,   151,
       6,     7,    11,    12,     1,    29,   149,   160,    18,     6,
       7,     8,     1,    36,     3,   168,     3,     6,     7,   162,
       9,    18,    36,   176,    90,    91,     9,   145,   145,   172,
     145,   174,     1,    36,     3,    36,    29,     1,    29,     3,
       9,    10,   185,    29,    13,     9,    10,    29,    29,    13,
      19,    20,    21,    22,     3,    19,    20,    21,    22,     1,
      36,     3,     5,    24,     1,    24,     3,     9,    10,    31,
      32,    13,     9,    10,    39,    37,    13,    19,    20,    21,
      22,    29,    19,    20,    21,    22,     1,    39,     3,    29,
       1,    37,     3,     3,     1,    10,     3,     8,    13,    10,
      29,     8,    37,    10,    19,    20,    21,    22,    19,    20,
      21,    22,    19,    20,    21,    22,     1,    35,    35,    14,
      29,     6,     7,     8,    25,    26,    27,    28,    31,    32,
      31,    32,    87,    18,    37,    37,    30,    92,    93,    40,
      41,    29,    29,    29,    11,    29,    29,    38,    12,    15,
      30,    29,    11,    16,    29,   121,   151,    39,   143,   176,
     164,    42,   141,    76
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,    43,    44,     0,     1,     6,     7,    18,    45,
      46,    52,    65,    67,    29,     3,     8,    46,    65,    67,
       3,    17,    47,     3,     1,     3,    10,    13,    19,    20,
      21,    22,    48,    49,    50,    53,    62,    64,    70,    71,
      72,    73,    30,     3,    29,     8,    29,    24,    38,    36,
       3,    36,    36,    36,     3,     4,     5,    32,    54,    55,
      56,    57,    58,     9,    50,    29,    29,    29,    29,    29,
      47,    36,     3,    46,    53,    65,    68,    69,    54,     3,
      56,    54,    61,    24,    54,    54,    54,    35,    36,     5,
      31,    32,    33,    34,    29,    52,    66,    29,     9,    69,
      39,    39,    25,    26,    27,    28,    40,    41,    37,    56,
      37,    37,    37,    57,    54,    59,    55,    55,    57,    57,
       3,    37,    29,    35,    35,    54,    54,    54,    54,    54,
      54,     1,     8,    51,    53,    62,    70,    71,    72,    73,
      14,    30,    37,    30,     8,    45,    54,    54,    29,    49,
      11,    12,    63,    29,    29,    29,    29,    29,    56,    59,
      38,    66,    49,     8,     9,     8,    51,    11,    15,    56,
      60,     9,    49,    63,    49,    56,    30,    39,    29,     9,
      11,     9,    16,    60,    29,    49,    29
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    42,    43,    44,    45,    45,    45,    45,    45,    45,
      46,    46,    47,    47,    48,    49,    49,    50,    50,    50,
      50,    50,    50,    50,    50,    51,    51,    51,    51,    51,
      51,    51,    52,    52,    53,    54,    54,    54,    55,    55,
      55,    56,    56,    57,    57,    57,    57,    57,    58,    59,
      59,    60,    60,    61,    61,    61,    61,    61,    61,    62,
      62,    62,    63,    63,    64,    65,    65,    66,    66,    67,
      68,    68,    69,    69,    69,    70,    70,    71,    72,    72,
      73,    73
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     6,     1,     2,     2,     2,     1,     1,     1,
       3,     2,     1,     3,     1,     2,     1,     2,     1,     1,
       2,     2,     2,     2,     2,     2,     1,     2,     2,     2,
       2,     2,     1,     1,     3,     3,     3,     1,     3,     3,
       1,     1,     2,     3,     1,     1,     1,     1,     7,     1,
       3,     1,     3,     3,     3,     3,     3,     3,     3,     9,
       7,     6,     4,     2,    11,    11,    10,     2,     4,     7,
       2,     1,     1,     1,     2,     6,     6,     4,     4,     4,
       2,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 11: /* var_decl: error ';'  */
#line 61 "src/grammar.y"
                    { yyerrok; }
#line 1389 "y.tab.c"
    break;

  case 24: /* simple_stmt: error ';'  */
#line 86 "src/grammar.y"
                    { yyerrok; }
#line 1395 "y.tab.c"
    break;

  case 31: /* single_stmt: error ';'  */
#line 96 "src/grammar.y"
                    { yyerrok; }
#line 1401 "y.tab.c"
    break;

  case 41: /* constant: TOKEN_CONST  */
#line 121 "src/grammar.y"
                      {
              long val = atol((yyvsp[0].str_val));
              if (val > 32767) {
                  yyerror("Error semantico: Constante positiva fuera de rango");
              }
          }
#line 1412 "y.tab.c"
    break;

  case 42: /* constant: '-' TOKEN_CONST  */
#line 127 "src/grammar.y"
                          {
              char neg_str[100];
              sprintf(neg_str, "-%s", (yyvsp[0].str_val));
              long val = atol(neg_str);
              if (val < -32768) {
                  yyerror("Error semantico: Constante negativa fuera de rango");
              } else {
                  add_to_symbol_table(neg_str, "INTEGER");
              }
          }
#line 1427 "y.tab.c"
    break;


#line 1431 "y.tab.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 229 "src/grammar.y"

