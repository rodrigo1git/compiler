#include <ctype.h>
#include "../include/stat_trans_mat.h"

int stat_mat[15][17] = {
    // L   i   s   D   _   .   $  +-   / op*()   =  <>  !:   "  nl  ws otro
    {  1,  1,  1,  5,  E,  7,  E,  F,  2,  F,   13, 14,  4, 12,  0,  0,  E }, // 0  - Estado inicial
    {  1,  1,  1,  1,  1,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }, // 1  - Identificadores
    {  F,  F,  F,  F,  F,  F,  F,  F,  3,  F,    F,  F,  F,  F,  F,  F,  F }, // 2  - Operador / o inicio comentario
    {  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,    3,  3,  3,  3,  0,  3,  3 }, // 3  - Comentario de línea
    {  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,    F,  E,  E,  E,  E,  E,  E }, // 4  - Operadores != y :=
    {  E,  E,  E,  5,  E,  7,  6,  E,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 5  - Constante entera
    {  E,  F,  E,  E,  E,  E,  E,  E,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 6  - Sufijo de entero ($i)
    {  E,  E,  E,  8,  E,  E,  E,  E,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 7  - Punto flotante
    {  F,  F,  9,  8,  F,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }, // 8  - Decimales flotante
    {  E,  E,  E, 11,  E,  E,  E, 10,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 9  - Exponente flotante ('s')
    {  E,  E,  E, 11,  E,  E,  E,  E,  E,  E,    E,  E,  E,  E,  E,  E,  E }, // 10 - Signo del exponente (+ o -)
    {  F,  F,  F, 11,  F,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }, // 11 - Dígitos del exponente
    { 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,   12, 12, 12,  F, 12, 12, 12 }, // 12 - Cadena de caracteres
    {  F,  F,  F,  F,  F,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }, // 13 - Operadores = y ==
    {  F,  F,  F,  F,  F,  F,  F,  F,  F,  F,    F,  F,  F,  F,  F,  F,  F }  // 14 - Operadores <, >, <=, >=
};

int get_col(int c) {
    if (c == 'i')
        return 1;
    else if (c == 's')
        return 2;
    else if (isalpha(c))
        return 0; // L (letras excluyendo 'i' y 's')
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
        return 14; // nl (Salto de línea y retorno de carro)
    else if (c == ' ' || c == '\t') 
        return 15; // ws (Espacio y tabulación)
    else 
        return 16; // otro
}