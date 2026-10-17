#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdbool.h>

extern int global_errors;
void set_trace_enabled(bool enabled);
void trace_printf(const char *format, ...);
void report_lexical_error(int line, const char *format, ...);
void report_semantic_error(int line, const char *format, ...);
void begin_lexical_recovery(void);
void begin_syntax_error(int line, const char *unexpected, const char *context);
void report_syntax_error(int line, const char *message);
void report_recovered_syntax_error(int line, const char *message);
void report_recovered_syntax_error_with_context(int line, const char *message);
void finish_error_recovery(void);
void finish_statement(void);
int is_lexical_recovery(void);
const char *syntax_error_unexpected(void);

#endif
