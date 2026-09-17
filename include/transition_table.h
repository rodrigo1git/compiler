#ifndef TRANSITION_TABLE_H
#define TRANSITION_TABLE_H

#define N_STATES  15
#define N_COLS    17

#define F   99    /* Final state */
#define E   (-1)  /* Error state */

/* Column indices matching get_col */
#define COL_L      0   /* Letters except 's' and 'i' */
#define COL_D      1   /* Digits */
#define COL_UND    2   /* _ */
#define COL_CMP    3   /* > < ! : */
#define COL_EQ     4   /* = */
#define COL_DOT    5   /* . */
#define COL_DOLLAR 6   /* $ */
#define COL_QUOTE  7   /* " */
#define COL_SLASH  8   /* / */
#define COL_ADD    9   /* + - */
#define COL_SYM   10   /* * ( ) ; */
#define COL_NL    11   /* \n */
#define COL_WS    12   /* whitespace */
#define COL_S     13   /* s */
#define COL_I     14   /* i */
#define COL_LBRK  15   /* [ */
#define COL_RBRK  16   /* ] */
#define COL_COMMA 17   /* , */
#define COL_OTHER 18   /* other */

extern int transition_table[N_STATES][N_COLS];

int get_col(int c);

#endif /* TRANSITION_TABLE_H */
