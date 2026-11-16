#ifndef TRANSITION_TABLE_H
#define TRANSITION_TABLE_H

#define N_STATES  15
#define N_COLS    17

#define F_CONS 99
#define F_RET  100
#define E   (-1)
#define ST_CHAIN 12

#define COL_L      0
#define COL_I      1
#define COL_S      2
#define COL_D      3
#define COL_UND    4
#define COL_DOT    5
#define COL_DOLLAR 6
#define COL_ADD    7
#define COL_SLASH  8
#define COL_SYM    9
#define COL_EQ    10
#define COL_CMP   11
#define COL_EXCL  12
#define COL_QUOTE 13
#define COL_NL    14
#define COL_WS    15
#define COL_OTHER 16

extern int transition_table[N_STATES][N_COLS];

int get_col(int c);

#endif
