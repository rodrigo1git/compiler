#include <ctype.h>
#include "../include/transition_table.h"

int transition_table[N_STATES][N_COLS] = {

    {  1,      1,      1,      5,      E,      7,      E,      F_CONS,   2,      F_CONS,   13,       14,       4,        12,     0,      0,      E },
    {  1,      1,      1,      1,      1,      F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET },
    {  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,    3,      F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET },
    {  3,      3,      3,      3,      3,      3,      3,      3,        3,      3,        3,        3,        3,        3,      0,      3,      3 },
    {  E,      E,      E,      E,      E,      E,      E,      E,        E,      E,        F_CONS,   E,        E,        E,      E,      E,      E },
    {  E,      E,      E,      5,      E,      7,      6,      E,        E,      E,        E,        E,        E,        E,      E,      E,      E },
    {  E,      F_CONS, E,      E,      E,      E,      E,      E,        E,      E,        E,        E,        E,        E,      E,      E,      E },
    {  F_RET,  F_RET,  F_RET,  8,      F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET },
    {  F_RET,  F_RET,  9,      8,      F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET },
    {  E,      E,      E,      11,     E,      E,      E,      10,       E,      E,        E,        E,        E,        E,      E,      E,      E },
    {  E,      E,      E,      11,     E,      E,      E,      E,        E,      E,        E,        E,        E,        E,      E,      E,      E },
    {  F_RET,  F_RET,  F_RET,  11,     F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_RET,    F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET },
    {  12,     12,     12,     12,     12,     12,     12,     12,       12,     12,       12,       12,       12,       F_CONS, 12,     12,     12 },
    {  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_CONS,   F_RET,   F_RET,    F_RET,  F_RET,  F_RET,  F_RET },
    {  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,  F_RET,    F_RET,  F_RET,    F_CONS,   F_RET,    F_RET,    F_RET,  F_RET,  F_RET,  F_RET }
};

int get_col(int c) {
    if (c == 'i')
        return COL_I;
    else if (c == 's')
        return COL_S;
    else if (isalpha(c))
        return COL_L;
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
