#ifndef TRANSITION_TABLE_H
#define TRANSITION_TABLE_H

#define N_STATES  15
#define N_COLS    17

#define F   99    /* Final state */
#define E   (-1)  /* Error state */

/* Column indices matching get_col */
#define COL_L      0   /* letters except 'i' and 's' */
#define COL_I      1   /* i */
#define COL_S      2   /* s */
#define COL_D      3   /* digits */
#define COL_UND    4   /* _ */
#define COL_DOT    5   /* . */
#define COL_DOLLAR 6   /* $ */
#define COL_ADD    7   /* + - */
#define COL_SLASH  8   /* / */
#define COL_SYM    9   /* * ( ) ; , [ ] */
#define COL_EQ    10   /* = */
#define COL_CMP   11   /* < > */
#define COL_EXCL  12   /* ! : */
#define COL_QUOTE 13   /* " */
#define COL_NL    14   /* newline */
#define COL_WS    15   /* whitespace */
#define COL_OTHER 16   /* other */

extern int transition_table[N_STATES][N_COLS];

int get_col(int c);

#endif /* TRANSITION_TABLE_H */
