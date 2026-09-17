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
        return 1;
    else if (c == 's')
        return 2;
    else if (isalpha(c))
        return 0; // letters except 'i' and 's'
    else if (isdigit(c))
        return 3; // D
    else if (c == '_')
        return 4; // _
    else if (c == '.')
        return 5; // .
    else if (c == '$')
        return 6; // $
    else if (c == '+' || c == '-')
        return 7; // +-
    else if (c == '/')
        return 8; // /
    else if (c == '*' || c == '(' || c == ')' || c == ';' || c == ',' || c == '[' || c == ']')
        return 9; // op*()
    else if (c == '=')
        return 10; // =
    else if (c == '<' || c == '>')
        return 11; // <>
    else if (c == '!' || c == ':')
        return 12; // !:
    else if (c == '"')
        return 13; // "
    else if (c == '\n' || c == '\r') 
        return 14; // newline
    else if (c == ' ' || c == '\t') 
        return 15; // whitespace
    else 
        return 16; // other
}