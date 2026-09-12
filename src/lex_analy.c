#include <stdio.h>
#include <ctype.h>
#include <string.h>

extern int yylex(){
  extern int yylval;

  int estado = 0;
  char c;
  char lexema[100]; // Buffer para lexema
  int ind = 0;


}
