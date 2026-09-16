#ifndef STAT_TRANS_MAT_H
#define STAT_TRANS_MAT_H

/* ------------------------------------------------------------------ */
/*  Automata de estados finitos del analizador lexico                 */
/*                                                                    */
/*  Se eliminaron los estados 14..17 (reconocimiento de listas         */
/*  [ n , n , n $i ]) porque armar la lista es trabajo del             */
/*  analizador sintactico: el lexico solo devuelve '[' ']' ','.        */
/*                                                                    */
/*  Quedan 14 estados (0..13) x 19 columnas.                           */
/* ------------------------------------------------------------------ */

#define N_STATES  15
#define N_COLS    17

#define F   99    /* estado final: el token ya se reconocio           */
#define E   (-1)  /* transicion invalida: error lexico                */

/* Columnas (el orden lo fija get_col) */
#define COL_L      0   /* letras (salvo 's' e 'i')      */
#define COL_D      1   /* digitos                       */
#define COL_UND    2   /* _                             */
#define COL_CMP    3   /* > < ! :                       */
#define COL_EQ     4   /* =                             */
#define COL_DOT    5   /* .                             */
#define COL_DOLLAR 6   /* $                             */
#define COL_QUOTE  7   /* "                             */
#define COL_SLASH  8   /* /                             */
#define COL_ADD    9   /* + -                           */
#define COL_SYM   10   /* * ( ) ;                       */
#define COL_NL    11   /* \n                            */
#define COL_WS    12   /* espacio, tab, \r              */
#define COL_S     13   /* s                             */
#define COL_I     14   /* i                             */
#define COL_LBRK  15   /* [                             */
#define COL_RBRK  16   /* ]                             */
#define COL_COMMA 17   /* ,                             */
#define COL_OTHER 18   /* cualquier otro caracter       */

extern int stat_mat[N_STATES][N_COLS];

int get_col(int c);

#endif /* STAT_TRANS_MAT_H */