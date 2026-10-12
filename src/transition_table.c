#include <ctype.h>
#include "../include/transition_table.h"

int transition_table[N_STATES][N_COLS] = {
    // L       i       s       D       _       .       $       +-        /       op*()     =         <>        !:        "       nl      ws      other
    {  1,      1,      1,      5,      E,      7,      E,      F_CONS,   2,      F_CONS,   13,       14,       4,        12,     0,      0,      E }, // 0: start
    {  1,      1,      1,      1,      1,      F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET }, // 1: identifier
    {  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,    3,      F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET }, // 2: slash / comment start
    {  3,      3,      3,      3,      3,      3,      3,      3,        3,      3,        3,        3,        3,        3,      0,      3,      3 }, // 3: line comment
    {  E,      E,      E,      E,      E,      E,      E,      E,        E,      E,        F_CONS,   E,        E,        E,      E,      E,      E }, // 4: != and :=
    {  E,      E,      E,      5,      E,      7,      6,      E,        E,      E,        E,        E,        E,        E,      E,      E,      E }, // 5: integer constant
    {  E,      F_CONS, E,      E,      E,      E,      E,      E,        E,      E,        E,        E,        E,        E,      E,      E,      E }, // 6: integer suffix ($i)
    {  F_RET,  F_RET,  F_RET,  8,      F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET }, // 7: float dot / field access
    {  F_RET,  F_RET,  9,      8,      F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET }, // 8: float decimals
    {  E,      E,      E,      11,     E,      E,      E,      10,       E,      E,        E,        E,        E,        E,      E,      E,      E }, // 9: float exponent ('s')
    {  E,      E,      E,      11,     E,      E,      E,      E,        E,      E,        E,        E,        E,        E,      E,      E,      E }, // 10: exponent sign (+/-)
    {  F_RET,  F_RET,  F_RET,  11,     F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET }, // 11: exponent digits
    {  12,     12,     12,     12,     12,     12,     12,     12,       12,     12,       12,       12,       12,       F_CONS, 12,     12,     12 }, // 12: chain literal
    {  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_CONS,   F_CONS,   F_RET,    F_RET,  F_RET,  F_RET,  F_RET }, // 13: = and ==
    {  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_CONS,   F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET }  // 14: relational ops
};

int get_col(int c) {
    if (c == 'i')
        return COL_I;
    else if (c == 's')
        return COL_S;
    else if (isalpha(c))
        return COL_L; // letters except 'i' and 's'
    else if (isdigit(c))
        return COL_D;
    else if (c == '_')
        return COL_UND;
    else if (c == '.')
        return COL_DOT;
    else if (c == '$')
        return COL_DOLLAR;
    else if (c == '+' || c == '-')
        return COL_ADD;
    else if (c == '/')
        return COL_SLASH;
    else if (c == '*' || c == '(' || c == ')' || c == ';' || c == ',' || c == '[' || c == ']')
        return COL_SYM;
    else if (c == '=')
        return COL_EQ;
    else if (c == '<' || c == '>')
        return COL_CMP;
    else if (c == '!' || c == ':')
        return COL_EXCL;
    else if (c == '"')
        return COL_QUOTE;
    else if (c == '\n' || c == '\r') 
        return COL_NL;
    else if (c == ' ' || c == '\t') 
        return COL_WS;
    else 
        return COL_OTHER;
}
