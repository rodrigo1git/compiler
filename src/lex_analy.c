#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "stat_trans_mat.h"
#include "sa_mat.h"

extern int yylval;

int state = 0;

extern int yylex(){
  char c;
  char lexema[100]; // Buffer para lexema
  int idx = 0;
  int tokken = -1;
  int col;

  while(tokken == -1){
    c = fgetc(s_file);
    
    if (c == EOF)
        return 0;
    
    col = get_col(c);
    
    sem_act = sem_act_mat[state][col];
    tokken = sem_act_t(c);

    state = stat_mat[state][col];
  }

  return tokken;
}