#define F 99
#define E -1
#include <ctype.h>

int stat_matrix[18][19] = {
    { 1,  5,  E,  4, 13,  7,  E, 12,  2,  F,  F,  0,  0,  E,  E, 14,  E,  F,  E},
    { 1,  1,  1,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  F},
    { E,  E,  E,  E,  E,  E,  E,  E,  3,  E,  E,  E,  E,  E,  E,  E,  E,  E,  F},
    { E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  0,  E,  E,  E,  E,  E,  E,  3},
    { E,  E,  E,  E,  F,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  F},
    { E,  5,  E,  E,  E,  7,  6,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E},
    { E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  F,  E,  E,  E,  E},
    { E,  8,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E},
    { E,  8,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  9,  E,  E,  E,  E,  F},
    { E, 11,  E,  E,  E,  E,  E,  E,  E, 10,  E,  E,  E,  E,  E,  E,  E,  E,  E},
    { E, 11,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E},
    { E, 11,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  F},
    { E,  E,  E,  E,  E,  E,  E,  F,  E,  E,  E, 12,  E,  E,  E,  E,  E,  E,  E},
    { E,  E,  E,  F,  F,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E},
    { E, 14,  E,  E,  E,  E, 15,  E,  E,  E,  E,  E,  E,  E,  E,  E,  F, 17,  E},
    { E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E, 17,  E,  E,  E,  E},
    { E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  F,  E,  E},
    { E, 14,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E}
};

int get_col(char c){
    if(isalpha(c))
        return 0;
    else if(isdigit(c))
        return 1;
    else if(c == '_')
        return 2;
    else if(c == '>' || c == '<' || c == '!' || c == ':')
        return 3;
    else if(c == '=')
        return 4;
    else if(c == '.')
        return 5;
    else if(c == '$')
        return 6;
    else if(c == '"')
        return 7;
    else if(c == '/')
        return 8;
    else if(c == '+' || c == '-')
        return 9;
    else if(c == '*' || c == '(' || c == ')' || c == ';')
        return 10;
    else if(c == 10)
        return 11;
    else if(c == 32 || c == 9)
        return 12;
    else if(c == 's')
        return 13;
    else if(c == 'i')
        return 14;
    else if(c == '[')
        return 15;
    else if(c == ']')
        return 16;
    else if(c == ',')
        return 17;
    else return 18;
};