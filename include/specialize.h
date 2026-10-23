#ifndef SPECIALIZE_H
#define SPECIALIZE_H

#include "rpn.h"

void specialize_init(void);
void specialize_register(map_node_t *symbol, const rpn_t *body);
bool specialize_program(const rpn_t *program);
bool specialize_print(FILE *stream);
void specialize_destroy(void);

#endif
