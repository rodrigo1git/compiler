#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../include/diagnostics.h"

static struct {
    bool pending;
    bool lexical;
    int line;
    const char *unexpected;
    char context[256];
} recovery;

int global_errors = 0;

static void flush_syntax_error(void) {
    if (recovery.pending) {
        fprintf(stderr, "Line %d: Syntax error: %s\n", recovery.line,
                recovery.context[0] ? recovery.context : "Invalid syntax");
        recovery.pending = false;
    }
}

void finish_error_recovery(void) {
    flush_syntax_error();
    memset(&recovery, 0, sizeof(recovery));
}

void report_lexical_error(int line, const char *format, ...) {
    va_list args;
    fprintf(stderr, "Line %d: Lexical error: ", line);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    global_errors++;
}

void begin_lexical_recovery(void) {
    /* Reporting a range error in a parser action does not return YYerror. */
    flush_syntax_error();
    memset(&recovery, 0, sizeof(recovery));
    recovery.lexical = true;
}

void begin_syntax_error(int line, const char *unexpected, const char *context) {
    finish_error_recovery();
    recovery.pending = true;
    recovery.line = line;
    recovery.unexpected = unexpected;
    snprintf(recovery.context, sizeof(recovery.context), "%s", context);
    /* Count at detection, even if no recovery production can reduce. */
    global_errors++;
}

void report_syntax_error(int line, const char *message) {
    /* Diagnostic productions without `error` detect independent problems. */
    global_errors++;
    fprintf(stderr, "Line %d: Syntax error: %s\n", line, message);
}

void report_recovered_syntax_error(int line, const char *message) {
    /* Bison already counted this error; YYerror was reported by the lexer. */
    if (!recovery.pending) return;
    fprintf(stderr, "Line %d: Syntax error: %s\n", line, message);
    recovery.pending = false;
}

void report_recovered_syntax_error_with_context(int line, const char *message) {
    char detailed[512];
    if (recovery.pending && recovery.context[0]) {
        snprintf(detailed, sizeof(detailed), "%s (%s)", message, recovery.context);
        report_recovered_syntax_error(line, detailed);
    } else {
        report_recovered_syntax_error(line, message);
    }
}

const char *syntax_error_unexpected(void) {
    return recovery.unexpected;
}

void finish_statement(void) {
    if (!recovery.pending && !recovery.lexical) {
        finish_error_recovery();
    }
}

int is_lexical_recovery(void) {
    return recovery.lexical;
}
