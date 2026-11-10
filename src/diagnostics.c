#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/diagnostics.h"
#include "../include/memory.h"

static struct {
    bool pending;
    bool lexical;
    int line;
    const char *unexpected;
    char *context;
} recovery;

int global_errors = 0;
static bool trace_enabled = true;

typedef struct semantic_diagnostic {
    semantic_origin_t origin;
    int line;
    char *message;
    struct semantic_diagnostic *next;
} semantic_diagnostic_t;

static semantic_origin_t semantic_origin;
static semantic_diagnostic_t *semantic_diagnostics;

semantic_origin_t set_semantic_origin(semantic_origin_t origin) {
    semantic_origin_t previous = semantic_origin;
    semantic_origin = origin;
    return previous;
}

void clear_semantic_origins(void) {
    while (semantic_diagnostics != NULL) {
        semantic_diagnostic_t *next = semantic_diagnostics->next;
        free(semantic_diagnostics->message);
        free(semantic_diagnostics);
        semantic_diagnostics = next;
    }
    semantic_origin = (semantic_origin_t){0};
}

void set_trace_enabled(bool enabled) {
    trace_enabled = enabled;
}

void trace_printf(const char *format, ...) {
    if (!trace_enabled) return;
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

static char *format_message(const char *format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    char *message = length < 0 ? NULL : malloc((size_t)length + 1);
    if (message == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate diagnostic message\n");
        exit(1);
    }
    vsnprintf(message, (size_t)length + 1, format, args);
    return message;
}

void diagnostic_append(char **message, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char *suffix = format_message(format, args);
    va_end(args);
    size_t used = *message ? strlen(*message) : 0;
    size_t length = memory_add(strlen(suffix), 1);
    *message = memory_resize(*message, memory_add(used, length));
    memcpy(*message + used, suffix, length);
    free(suffix);
}

static void write_diagnostic(const char *category, int line, const char *message) {
    fprintf(stderr, "Line %d: ", line);
    if (category != NULL) fprintf(stderr, "%s error: ", category);
    fprintf(stderr, "%s\n", message);
}

static bool register_semantic_error(int line, const char *message) {
    if (semantic_origin.body == NULL) return true;
    for (semantic_diagnostic_t *previous = semantic_diagnostics; previous != NULL;
         previous = previous->next) {
        if (previous->origin.body == semantic_origin.body &&
            previous->origin.step == semantic_origin.step && previous->line == line &&
            strcmp(previous->message, message) == 0) return false;
    }
    semantic_diagnostic_t *entry = malloc(sizeof(*entry));
    char *saved_message = strdup(message);
    if (entry == NULL || saved_message == NULL) {
        fprintf(stderr, "Fatal error: Cannot allocate semantic diagnostic\n");
        exit(1);
    }
    *entry = (semantic_diagnostic_t){semantic_origin, line, saved_message, semantic_diagnostics};
    semantic_diagnostics = entry;
    return true;
}

void report_semantic_error(int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char *message = format_message(format, args);
    va_end(args);
    if (register_semantic_error(line, message)) {
        write_diagnostic("Semantic", line, message);
        global_errors++;
    }
    free(message);
}

static void flush_syntax_error(void) {
    if (recovery.pending) {
        write_diagnostic("Syntax", recovery.line,
                         recovery.context && recovery.context[0] ? recovery.context : "Invalid syntax");
        recovery.pending = false;
    }
}

void finish_error_recovery(void) {
    flush_syntax_error();
    free(recovery.context);
    memset(&recovery, 0, sizeof(recovery));
}

void report_lexical_error(int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char *message = format_message(format, args);
    va_end(args);
    write_diagnostic("Lexical", line, message);
    free(message);
    global_errors++;
}

void report_parser_error(int line, const char *message) {
    finish_error_recovery();
    global_errors++;
    write_diagnostic(NULL, line, message);
}

void begin_lexical_recovery(void) {
    /* Reporting a range error in a parser action does not return YYerror. */
    finish_error_recovery();
    recovery.lexical = true;
}

void begin_syntax_error(int line, const char *unexpected, const char *context) {
    finish_error_recovery();
    recovery.pending = true;
    recovery.line = line;
    recovery.unexpected = unexpected;
    diagnostic_append(&recovery.context, "%s", context ? context : "");
    /* Count at detection, even if no recovery production can reduce. */
    global_errors++;
}

void report_syntax_error(int line, const char *message) {
    /* Diagnostic productions without `error` detect independent problems. */
    global_errors++;
    write_diagnostic("Syntax", line, message);
}

void report_recovered_syntax_error(int line, const char *message) {
    /* Bison already counted this error; YYerror was reported by the lexer. */
    if (!recovery.pending) return;
    write_diagnostic("Syntax", line, message);
    recovery.pending = false;
}

void report_recovered_syntax_error_with_context(int line, const char *message) {
    if (recovery.pending && recovery.context && recovery.context[0]) {
        char *detailed = NULL;
        diagnostic_append(&detailed, "%s (%s)", message, recovery.context);
        report_recovered_syntax_error(line, detailed);
        free(detailed);
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
