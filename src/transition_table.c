#include <ctype.h>
#include "../include/transition_table.h"

int transition_table[15][17] = {
    // L   i   s   D   _   .   $  +-   / op*()   =  <>  !:   "  nl  ws otro
    {  1,  1,  1,  5,  E,  7,  E,  F,  2,  F,   13, 14,  4, 12,  0,  0,  E }, // 0: start
    {  1,  1,  1,  1,  1,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }, // 1: identifier
    {  F,  F,  F,  F,  F,  F,  F,  F,  3,  F,    F,  F,  F,  F,  F,  F,  F }, // 2: slash / comment start
    {  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,    3,  3,  3,  3,  0,  3,  3 }, // 3: line comment
    {  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,    F,  E,  E,  E,  E,  E,  E }, // 4: != and :=
    {  E,  E,  E,  5,  E,  7,  6,  E,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 5: integer constant
    {  E,  F,  E,  E,  E,  E,  E,  E,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 6: integer suffix ($i)
    {  E,  E,  E,  8,  E,  E,  E,  E,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 7: float dot
    {  F,  F,  9,  8,  F,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }, // 8: float decimals
    {  E,  E,  E, 11,  E,  E,  E, 10,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 9: float exponent ('s')
    {  E,  E,  E, 11,  E,  E,  E,  E,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 10: exponent sign (+/-)
    {  F,  F,  F, 11,  F,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }, // 11: exponent digits
    { 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,   12, 12, 12,  F, 12, 12, 12 }, // 12: string literal
    {  F,  F,  F,  F,  F,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }, // 13: = and ==
    {  F,  F,  F,  F,  F,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }  // 14: relational ops
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