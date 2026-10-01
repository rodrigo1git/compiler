#ifndef LEXER_H
#define LEXER_H
#include "../y.tab.h"

extern int yylex(void);
extern int prev_token;
extern int cur_token;

#endif /* LEXER_H */
