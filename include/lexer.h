#ifndef LEXER_H
#define LEXER_H
#include "../y.tab.h"

extern int yylex(void);
extern int current_line;
extern void lexer_reset_input(void);

#endif /* LEXER_H */
