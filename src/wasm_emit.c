#include "../include/wasm_emit.h"
#include "../include/wasm_internal.h"
#include "../include/specialize.h"
#include "../include/diagnostics.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_module(wasm_module_t *mod);
static void write_module_header(wasm_module_t *mod);

bool wasm_write(const char *path) {
    wasm_module_t mod = {0};
    if (!layout_init(&mod.layout)) {
        layout_destroy(&mod.layout);
        return false;
    }

    wasm_prepare_bodies(&mod);
    if (mod.layout.chains_end + mod.bodies[0].size > WASM32_MEMORY_BYTES) {
        report_semantic_error(1, "Global storage is not representable in wasm32");
    }

    char *temporary = malloc(strlen(path) + 16);
    if (!temporary) {
        exit(1);
    }
    sprintf(temporary, "%s.tmp.XXXXXX", path);

    int descriptor = -1;
    if (!global_errors) {
        descriptor = mkstemp(temporary);
    }
    if (descriptor >= 0) {
        mod.out = fdopen(descriptor, "w");
        if (!mod.out) {
            close(descriptor);
        }
    }

    bool success = mod.out != NULL;
    if (success) {
        write_module(&mod);
        success = !global_errors && !ferror(mod.out);
        if (fclose(mod.out)) {
            success = false;
        }
        if (success && rename(temporary, path)) {
            success = false;
        }
    }
    if (!success) {
        unlink(temporary);
        if (!global_errors) {
            report_semantic_error(1, "Cannot write WebAssembly file '%s'", path);
        }
    }

    for (size_t i = 0; i < mod.count; i++) {
        free(mod.bodies[i].scratch);
        free(mod.bodies[i].controls);
    }
    free(mod.bodies);
    free(temporary);
    layout_destroy(&mod.layout);
    return success;
}

static void write_module(wasm_module_t *mod) {
    wasm_emit_line(mod->out, 0, "(module");
    write_module_header(mod);
    wasm_emit_runtime(mod);
    for (size_t i = 0; i < mod->count; i++) {
        wasm_write_body(mod, &mod->bodies[i]);
    }
    wasm_emit_line(mod->out, 0, ")");
}

static void write_module_header(wasm_module_t *mod) {
    FILE *out = mod->out;
    wasm_emit_line(out, 1, "(import \"env\" \"printInteger\" (func $print_integer (param i32)))");
    wasm_emit_line(out, 1, "(import \"env\" \"printFloat\" (func $print_float (param f32)))");
    wasm_emit_line(out, 1, "(import \"env\" \"printString\" (func $print_chain (param i32)))");
    wasm_emit_line(out, 1, "(import \"env\" \"runtimeFail\" (func $fail (param i32 i32)))");
    wasm_emit_line(out, 1, "(memory (export \"memory\") %" PRIu64 ")",
                   (mod->layout.chains_end + WASM_PAGE_BYTES - 1) / WASM_PAGE_BYTES);
    wasm_emit_line(out, 1, "(global $sp (mut i64) (i64.const %" PRIu64 "))", mod->layout.chains_end);

    for (size_t i = 0; i < specialize_source_count(); i++) {
        map_node_t *symbol;
        specialize_source_body(i, &symbol);
        wasm_emit_line(out, 1, "(global $active_%zu (mut i32) (i32.const 0))", layout_id(&mod->layout, symbol));
    }

    for (size_t i = 0; i < mod->layout.count; i++) {
        const map_node_t *s = mod->layout.items[i].symbol;
        if (s->usage != SYMBOL_LEXEME || strcmp(s->value, "CHAIN")) {
            continue;
        }
        fprintf(out, "  (data (i32.const %" PRIu64 ") \"", mod->layout.items[i].offset);
        for (const unsigned char *p = (const unsigned char *)s->key; *p; p++) {
            fprintf(out, "\\%02x", *p);
        }
        fputs("\\00\")\n", out);
    }
}

static void emit_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) {
        fputs("  ", out);
    }
}

void wasm_emit_line(FILE *out, int indent, const char *format, ...) {
    emit_indent(out, indent);
    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);
    fputc('\n', out);
}

void wasm_emit_block(FILE *out, int indent, const char *format, ...) {
    char stack_buf[4096];
    char *heap_buf = NULL;
    char *buf = stack_buf;

    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(stack_buf, sizeof(stack_buf), format, args);
    va_end(args);

    if (needed < 0) {
        va_end(args_copy);
        return;
    }
    if ((size_t)needed >= sizeof(stack_buf)) {
        heap_buf = malloc((size_t)needed + 1);
        if (!heap_buf) {
            va_end(args_copy);
            return;
        }
        vsnprintf(heap_buf, (size_t)needed + 1, format, args_copy);
        buf = heap_buf;
    }
    va_end(args_copy);

    const char *cursor = buf;
    while (1) {
        const char *newline = strchr(cursor, '\n');
        size_t len = newline ? (size_t)(newline - cursor) : strlen(cursor);
        if (len > 0) {
            emit_indent(out, indent);
            fwrite(cursor, 1, len, out);
        }
        fputc('\n', out);
        if (!newline) {
            break;
        }
        cursor = newline + 1;
    }

    free(heap_buf);
}
