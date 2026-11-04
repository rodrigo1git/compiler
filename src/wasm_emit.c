#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/wasm_emit.h"
#include "../include/layout.h"
#include "../include/specialize.h"
#include "../include/diagnostics.h"

#define MEMORY_LIMIT (16 * 1024 * 1024)
typedef struct { const rpn_t *code; map_node_t *symbol; size_t size; size_t *scratch; } body_t;
typedef struct { type_t type; size_t local, tag; } operand_t;
typedef struct { const map_node_t *symbol; size_t local; } call_t;
typedef struct {
    FILE *out;
    layout_t layout;
    body_t *bodies;
    size_t count;
    body_t *body;
    operand_t *stack;
    size_t depth;
    call_t *calls;
    size_t call_depth;
    size_t locals;
    type_kind_t *local_types;
    bool planning;
} emitter_t;

static void write_line(emitter_t *e, const char *format, ...) {
    if (e->planning) return;
    va_list args;
    va_start(args, format);
    vfprintf(e->out, format, args);
    va_end(args);
    fputc('\n', e->out);
}
static void *allocate(size_t count, size_t size) {
    void *p = calloc(count ? count : 1, size);
    if (!p) { fprintf(stderr, "Cannot allocate WebAssembly generation state\n"); exit(1); }
    return p;
}
static size_t body_id(emitter_t *e) { return layout_id(&e->layout, e->body->symbol); }
static const char *wasm_type(type_t type) { return type.kind == TYPE_SINGLEF ? "f32" : "i32"; }
static size_t new_local(emitter_t *e, type_t type, int line) {
    size_t id = ++e->locals;
    if (e->planning) {
        e->local_types[id] = type.kind;
        char key[96];
        snprintf(key, sizeof(key), "@tmp_%zu_%zu", body_id(e), id);
        map_node_t *s = map_put(symbol_table, key, "ID");
        s->usage = SYMBOL_AUXILIARY;
        s->owner = (map_node_t *)layout_source(e->body->symbol);
        s->declaration_line = line;
        s->type_symbol = (map_node_t *)type.class_symbol;
        if (!s->declared_type) s->declared_type = strdup(type_name(type));
    }
    return id;
}
static size_t save_local(emitter_t *e, type_t type, int line) {
    size_t id = new_local(e, type, line);
    write_line(e, "local.set $tmp_%zu_%zu", body_id(e), id);
    return id;
}
static void get_local(emitter_t *e, size_t id) { write_line(e, "local.get $tmp_%zu_%zu", body_id(e), id); }
static operand_t pop(emitter_t *e) {
    if (!e->depth) { report_semantic_error(1, "Invalid WebAssembly translation stack"); return (operand_t){0}; }
    return e->stack[--e->depth];
}
static void push(emitter_t *e, operand_t value) { e->stack[e->depth++] = value; }
static void fail_if(emitter_t *e, int code, int line) {
    write_line(e, "if\ni32.const %d\ni32.const %d\ncall $fail\nunreachable\nend", code, line);
}
static type_t integer_type(void) { return (type_t){TYPE_INTEGER, NULL}; }
static void frame_address(emitter_t *e, const map_node_t *owner) {
    const map_node_t *current = layout_source(e->body->symbol);
    owner = layout_source(owner);
    write_line(e, "local.get $frame");
    while (current && current != owner) {
        write_line(e, "i32.load");
        current = layout_parent(current);
    }
    if (current != owner) report_semantic_error(owner ? owner->declaration_line : 1, "Cannot locate lexical frame");
}
static void receiver_address(emitter_t *e) {
    const map_node_t *owner = layout_source(e->body->symbol);
    while (owner && owner->usage != SYMBOL_METHOD) owner = layout_parent(owner);
    if (!owner) { report_semantic_error(1, "Missing method receiver"); write_line(e, "i32.const 0"); return; }
    frame_address(e, owner);
    write_line(e, "i32.load offset=4");
}
static void symbol_address(emitter_t *e, const map_node_t *symbol, int line) {
    if (symbol->usage == SYMBOL_ATTRIBUTE) {
        receiver_address(e);
        write_line(e, "i32.const %zu\ni32.const %d\ncall $field", layout_id(&e->layout, symbol), line);
    } else {
        frame_address(e, symbol->owner);
        write_line(e, "i32.const %zu\ni32.add", layout_offset(&e->layout, symbol));
    }
}
static bool object_type(type_t type) {
    if (type.kind == TYPE_OBJECT) return true;
    if (type.kind != TYPE_CANDIDATES) return false;
    for (size_t i = 0; i < type.class_symbol->effective_attributes.count; i++)
        if (type_of_symbol(type.class_symbol->effective_attributes.items[i]).kind != TYPE_OBJECT) return false;
    return true;
}
static size_t snapshot(emitter_t *e, size_t source, size_t pc) {
    write_line(e, "local.get $frame\ni32.const %zu\ni32.add", e->body->scratch[pc]);
    size_t result = save_local(e, integer_type(), e->body->code->steps[pc].source_line);
    get_local(e, result); get_local(e, source); get_local(e, source);
    write_line(e, "call $object_size\nmemory.copy");
    return result;
}
static operand_t read_symbol(emitter_t *e, const map_node_t *s, size_t pc) {
    int line = e->body->code->steps[pc].source_line;
    type_t type = type_of_symbol(s);
    if (s->usage == SYMBOL_LEXEME) {
        if (type.kind == TYPE_STRING) write_line(e, "i32.const %zu", layout_offset(&e->layout, s));
        else if (type.kind == TYPE_INTEGER) write_line(e, "i32.const %ld", strtol(s->key, NULL, 10));
        else {
            char *text = strdup(s->key);
            for (char *p = text; *p; p++) if (*p == 's') *p = 'e';
            write_line(e, "f32.const %s", text); free(text);
        }
    } else {
        symbol_address(e, s, line);
        if (type.kind != TYPE_OBJECT) write_line(e, "%s.load", wasm_type(type));
    }
    size_t local = save_local(e, type, line);
    if (type.kind == TYPE_OBJECT) local = snapshot(e, local, pc);
    return (operand_t){type, local, 0};
}
static void materialize(emitter_t *e, size_t object, const map_node_t *capture, size_t offset);

static void attribute(emitter_t *e, const rpn_step_t *step, size_t pc) {
    type_t type = type_of_attribute(step);
    operand_t index = read_symbol(e, step->index, pc);
    symbol_address(e, step->receiver, step->source_line);
    size_t object = save_local(e, integer_type(), step->source_line);
    if (step->capture) {
        object = snapshot(e, object, pc);
        materialize(e, object, step->capture, 0);
    }
    get_local(e, object); get_local(e, index.local);
    write_line(e, "i32.const %d\ncall $position", step->source_line);
    size_t address = save_local(e, integer_type(), step->source_line);
    bool destination = step->kind == RPN_ATTRIBUTE_DESTINATION;
    size_t local = address, tag = 0;
    if (!destination) {
        if (object_type(type)) local = snapshot(e, address, pc);
        else {
            if (type.kind == TYPE_CANDIDATES) {
                get_local(e, object); get_local(e, index.local);
                write_line(e, "i32.const %d\ncall $field_type", step->source_line);
                tag = save_local(e, integer_type(), step->source_line);
            }
            get_local(e, address);
            write_line(e, "%s.load", wasm_type(type));
            local = save_local(e, type, step->source_line);
        }
    }
    push(e, (operand_t){type, local, tag});
}
static body_t *find_body(emitter_t *e, const map_node_t *symbol) {
    for (size_t i = 1; i < e->count; i++) if (e->bodies[i].symbol == symbol) return &e->bodies[i];
    report_semantic_error(symbol->declaration_line, "Missing executable body '%s'", symbol->key);
    return &e->bodies[0];
}
static void initialize_frame(emitter_t *e, size_t local, const map_node_t *function) {
    const map_node_t *source = layout_source(function);
    for (size_t i = 0; i < e->layout.count; i++) {
        const map_node_t *s = e->layout.items[i].symbol;
        if ((s->usage == SYMBOL_VARIABLE || s->usage == SYMBOL_PARAMETER) && s->owner == source && s->type_symbol) {
            get_local(e, local);
            write_line(e, "i32.const %zu\ni32.add\ncall $init_%zu", layout_offset(&e->layout, s), layout_id(&e->layout, s->type_symbol));
        }
    }
}
static void begin_call(emitter_t *e, const rpn_step_t *step) {
    body_t *body = find_body(e, step->value.symbol);
    write_line(e, "i32.const %zu\ni32.const %d\ncall $allocate", body->size, step->source_line);
    size_t frame = save_local(e, integer_type(), step->source_line);
    get_local(e, frame); frame_address(e, layout_parent(step->value.symbol)); write_line(e, "i32.store");
    const map_node_t *source = layout_source(step->value.symbol);
    if (source->usage == SYMBOL_METHOD) {
        get_local(e, frame);
        if (step->receiver) symbol_address(e, step->receiver, step->source_line);
        else receiver_address(e);
        write_line(e, "i32.store offset=4");
    }
    initialize_frame(e, frame, step->value.symbol);
    e->calls[e->call_depth++] = (call_t){step->value.symbol, frame};
}
static void call_function(emitter_t *e, const rpn_step_t *step) {
    call_t call = e->calls[--e->call_depth];
    get_local(e, call.local);
    write_line(e, "call $fn_%zu", layout_id(&e->layout, step->value.symbol));
    type_t type = type_of_symbol(step->value.symbol);
    size_t result = save_local(e, type, step->source_line);
    get_local(e, call.local); write_line(e, "global.set $sp");
    push(e, (operand_t){type, result, 0});
}
static void materialize(emitter_t *e, size_t object, const map_node_t *capture, size_t offset) {
    const map_node_t *kind = capture->type_symbol;
    for (size_t i = 0; i < kind->effective_attributes.count; i++) {
        const map_node_t *field = kind->effective_attributes.items[i];
        size_t field_offset = offset + layout_field_offset(&e->layout, kind, i);
        for (size_t j = 0; j < capture->comptime_bindings.count; j++)
            if (capture->comptime_bindings.items[j] == field) {
                const map_node_t *constant = capture->comptime_values.items[j];
                get_local(e, object); write_line(e, "i32.const %zu\ni32.add", field_offset);
                if (type_of_symbol(field).kind == TYPE_INTEGER) write_line(e, "i32.const %ld", strtol(constant->key, NULL, 10));
                else {
                    char *text = strdup(constant->key);
                    for (char *p = text; *p; p++) if (*p == 's') *p = 'e';
                    write_line(e, "f32.const %s", text); free(text);
                }
                write_line(e, "%s.store", wasm_type(type_of_symbol(field)));
            }
        for (size_t j = 0; j < capture->object_fields.count; j++)
            if (capture->object_fields.items[j] == field)
                materialize(e, object, capture->object_values.items[j], field_offset);
    }
}

static void binary(emitter_t *e, rpn_operator_t op, int line) {
    operand_t right = pop(e), left = pop(e);
    bool floating = left.type.kind == TYPE_SINGLEF;
    static const char *integer_ops[] = {"add", "sub", "mul", "div_s", "", "lt_s", "gt_s", "le_s", "ge_s", "eq", "ne"};
    static const char *float_ops[] = {"add", "sub", "mul", "div", "", "lt", "gt", "le", "ge", "eq", "ne"};
    type_t result_type = op >= RPN_LESS ? integer_type() : left.type;
    if (floating && op == RPN_ADD) {
        get_local(e, left.local); write_line(e, "f64.promote_f32");
        get_local(e, right.local); write_line(e, "f64.promote_f32\nf64.add\nlocal.tee $wide\nf64.abs\nf64.const 3.40282347e38\nf64.ge");
        fail_if(e, 1, line);
        write_line(e, "local.get $wide\nf32.demote_f64");
    } else {
        get_local(e, left.local); get_local(e, right.local);
        write_line(e, "%s.%s", floating ? "f32" : "i32", floating ? float_ops[op] : integer_ops[op]);
    }
    size_t result = save_local(e, result_type, line);
    if (!floating && op == RPN_MULTIPLY) {
        get_local(e, result); write_line(e, "i32.const -32768\ni32.lt_s");
        get_local(e, result); write_line(e, "i32.const 32767\ni32.gt_s\ni32.or");
        fail_if(e, 2, line);
    }
    if (floating && op == RPN_ADD) {
        get_local(e, result); write_line(e, "f64.promote_f32\nf64.abs\nf64.const 3.40282347e38\nf64.ge");
        fail_if(e, 1, line);
    }
    if (!floating && op < RPN_ASSIGN) {
        get_local(e, result); write_line(e, "i32.const 16\ni32.shl\ni32.const 16\ni32.shr_s\nlocal.set $tmp_%zu_%zu", body_id(e), result);
    }
    push(e, (operand_t){result_type, result, 0});
}
static void convert_integer(emitter_t *e, int line) {
    operand_t value = pop(e);
    get_local(e, value.local); get_local(e, value.local); write_line(e, "f32.ne");
    get_local(e, value.local); write_line(e, "f32.const -32768\nf32.lt\ni32.or");
    get_local(e, value.local); write_line(e, "f32.const 32767\nf32.gt\ni32.or");
    fail_if(e, 4, line);
    get_local(e, value.local); get_local(e, value.local); write_line(e, "f32.trunc\nf32.ne");
    fail_if(e, 3, line);
    get_local(e, value.local); write_line(e, "i32.trunc_f32_s");
    push(e, (operand_t){integer_type(), save_local(e, integer_type(), line), 0});
}
static void output(emitter_t *e) {
    operand_t value = pop(e);
    if (value.type.kind == TYPE_CANDIDATES) {
        get_local(e, value.tag); write_line(e, "i32.const %d\ni32.eq\nif", TYPE_INTEGER);
        get_local(e, value.local); write_line(e, "call $print_integer\nelse");
        get_local(e, value.local); write_line(e, "f32.reinterpret_i32\ncall $print_float\nend");
    } else {
        get_local(e, value.local);
        write_line(e, "call $print_%s", value.type.kind == TYPE_SINGLEF ? "float" : value.type.kind == TYPE_STRING ? "string" : "integer");
    }
}
static void emit_operator(emitter_t *e, const rpn_step_t *step) {
    rpn_operator_t op = step->value.operator;
    if (op <= RPN_NOT_EQUAL && op != RPN_ASSIGN) binary(e, op, step->source_line);
    else if (op == RPN_ASSIGN) {
        operand_t destination = pop(e), value = pop(e);
        get_local(e, destination.local); get_local(e, value.local);
        if (object_type(value.type)) write_line(e, "call $copy_object");
        else write_line(e, "%s.store", wasm_type(value.type));
    } else if (op == RPN_DUPLICATE) { operand_t value = pop(e); push(e, value); push(e, value); }
    else if (op == RPN_DROP) pop(e);
    else if (op == RPN_TO_INTEGER) convert_integer(e, step->source_line);
    else if (op == RPN_OUTPUT) output(e);
    else if (op == RPN_RETURN) {
        operand_t value = pop(e); get_local(e, value.local); write_line(e, "local.set $result\nbr $return");
    } else report_semantic_error(step->source_line, "Unexpected branch outside structured region");
}
static void emit_range(emitter_t *e, size_t begin, size_t end);
static size_t emit_control(emitter_t *e, size_t begin, size_t target) {
    const rpn_t *body = e->body->code;
    const rpn_step_t *branch = &body->steps[target];
    size_t false_start = branch->value.target - 1, body_end = false_start, finish = false_start;
    bool has_else = false;
    if (false_start >= 2 && body->steps[false_start - 1].kind == RPN_OPERATOR &&
        body->steps[false_start - 1].value.operator == RPN_BRANCH) {
        body_end = false_start - 2;
        if (!branch->loop) {
            finish = body->steps[body_end].value.target - 1;
            has_else = true;
        }
    }
    if (branch->loop) write_line(e, "block $exit_%zu\nloop $loop_%zu", target, target);
    emit_range(e, begin, target);
    operand_t condition = pop(e); get_local(e, condition.local);
    if (branch->loop) {
        write_line(e, "i32.eqz\nbr_if $exit_%zu", target);
        emit_range(e, target + 2, body_end);
        write_line(e, "br $loop_%zu\nend\nend", target);
    } else {
        write_line(e, "if");
        emit_range(e, target + 2, body_end);
        if (has_else) { write_line(e, "else"); emit_range(e, false_start, finish); }
        write_line(e, "end");
    }
    return finish;
}
static void emit_step(emitter_t *e, size_t pc) {
    const rpn_step_t *step = &e->body->code->steps[pc];
    write_line(e, ";; RPN %zu, source line %d", pc + 1, step->source_line);
    switch (step->kind) {
    case RPN_SYMBOL: push(e, read_symbol(e, step->value.symbol, pc)); break;
    case RPN_DESTINATION: case RPN_ARGUMENT_DESTINATION: {
        if (step->kind == RPN_ARGUMENT_DESTINATION) {
            get_local(e, e->calls[e->call_depth - 1].local);
            write_line(e, "i32.const %zu\ni32.add", layout_offset(&e->layout, step->value.symbol));
        } else symbol_address(e, step->value.symbol, step->source_line);
        push(e, (operand_t){type_of_symbol(step->value.symbol), save_local(e, integer_type(), step->source_line), 0});
        break;
    }
    case RPN_ATTRIBUTE: case RPN_ATTRIBUTE_DESTINATION: attribute(e, step, pc); break;
    case RPN_INDEX_CHECK: {
        operand_t index = read_symbol(e, step->index, pc);
        symbol_address(e, step->receiver, step->source_line);
        get_local(e, index.local);
        write_line(e, "i32.const %d\ncall $position\ndrop", step->source_line);
        break;
    }
    case RPN_CALL_FRAME: begin_call(e, step); break;
    case RPN_CALL: call_function(e, step); break;
    case RPN_OPERATOR: emit_operator(e, step); break;
    case RPN_OBJECT_VALUE:
        if (e->depth) materialize(e, e->stack[e->depth - 1].local, step->value.symbol, 0);
        break;
    case RPN_TARGET: report_semantic_error(step->source_line, "Unstructured jump target in WebAssembly generation"); break;
    }
}
static void emit_range(emitter_t *e, size_t begin, size_t end) {
    for (size_t pc = begin; pc < end; pc++) {
        size_t target = pc;
        while (target < end && e->body->code->steps[target].control_start != pc + 1) target++;
        if (target < end) pc = emit_control(e, pc, target) - 1;
        else emit_step(e, pc);
    }
}

static void write_helpers(emitter_t *e) {
    write_line(e, "(func $allocate (param $size i32) (param $line i32) (result i32) (local $old i32)\n"
                  "global.get $sp\nlocal.tee $old\nlocal.get $size\ni32.add\ni32.const %d\ni32.gt_u\n"
                  "if\ni32.const 8\nlocal.get $line\ncall $fail\nunreachable\nend\n"
                  "local.get $old\ni32.const 0\nlocal.get $size\nmemory.fill\n"
                  "local.get $old\nlocal.get $size\ni32.add\nglobal.set $sp\nlocal.get $old)", MEMORY_LIMIT);
    write_line(e, "(func $object_size (param $object i32) (result i32)");
    for (size_t i = 0; i < e->layout.count; i++) {
        const layout_entry_t *item = &e->layout.items[i];
        if (item->symbol->usage != SYMBOL_CLASS) continue;
        write_line(e, "local.get $object\ni32.load\ni32.const %zu\ni32.eq\nif\ni32.const %zu\nreturn\nend", item->id, item->size);
    }
    write_line(e, "unreachable)");
    for (size_t i = 0; i < e->layout.count; i++) {
        const layout_entry_t *item = &e->layout.items[i];
        const map_node_t *kind = item->symbol;
        if (kind->usage != SYMBOL_CLASS) continue;
        write_line(e, "(func $init_%zu (param $object i32)\nlocal.get $object\ni32.const %zu\ni32.store", item->id, item->id);
        for (size_t j = 0; j < kind->effective_attributes.count; j++) {
            const map_node_t *field = kind->effective_attributes.items[j];
            if (field->type_symbol)
                write_line(e, "local.get $object\ni32.const %zu\ni32.add\ncall $init_%zu",
                    layout_field_offset(&e->layout, kind, j), layout_id(&e->layout, field->type_symbol));
        }
        write_line(e, ")");
    }
    for (int helper = 0; helper < 3; helper++) {
        write_line(e, "(func $%s (param $object i32) (param $index i32) (param $line i32) (result i32)",
            helper == 0 ? "position" : helper == 1 ? "field" : "field_type");
        for (size_t i = 0; i < e->layout.count; i++) {
            const map_node_t *kind = e->layout.items[i].symbol;
            if (kind->usage != SYMBOL_CLASS) continue;
            write_line(e, "local.get $object\ni32.load\ni32.const %zu\ni32.eq\nif", e->layout.items[i].id);
            for (size_t j = 0; j < kind->effective_attributes.count; j++) {
                const map_node_t *field = kind->effective_attributes.items[j];
                write_line(e, "local.get $index\ni32.const %zu\ni32.eq\nif",
                    helper == 1 ? layout_id(&e->layout, field) : j);
                if (helper == 2) write_line(e, "i32.const %d", type_of_symbol(field).kind);
                else write_line(e, "local.get $object\ni32.const %zu\ni32.add", layout_field_offset(&e->layout, kind, j));
                write_line(e, "return\nend");
            }
            write_line(e, "end");
        }
        write_line(e, "i32.const 5\nlocal.get $line\ncall $fail\nunreachable)");
    }
    write_line(e, "(func $copy_object (param $destination i32) (param $source i32)");
    for (size_t i = 0; i < e->layout.count; i++) {
        const map_node_t *dst = e->layout.items[i].symbol;
        if (dst->usage != SYMBOL_CLASS) continue;
        for (size_t j = 0; j < e->layout.count; j++) {
            const map_node_t *src = e->layout.items[j].symbol;
            if (src->usage != SYMBOL_CLASS || !types_compatible((type_t){TYPE_OBJECT, dst}, (type_t){TYPE_OBJECT, src})) continue;
            write_line(e, "local.get $destination\ni32.load\ni32.const %zu\ni32.eq\nlocal.get $source\ni32.load\ni32.const %zu\ni32.eq\ni32.and\nif",
                layout_id(&e->layout, dst), layout_id(&e->layout, src));
            for (size_t k = 0; k < dst->effective_attributes.count; k++) {
                const map_node_t *field = dst->effective_attributes.items[k];
                write_line(e, "local.get $destination\ni32.const %zu\ni32.add\nlocal.get $source\ni32.const %zu\ni32.add",
                    layout_field_offset(&e->layout, dst, k), layout_field_offset(&e->layout, src, k));
                write_line(e, field->type_symbol ? "call $copy_object" : "i32.load\ni32.store");
            }
            write_line(e, "return\nend");
        }
    }
    write_line(e, "unreachable)");
}

static void prepare_bodies(emitter_t *e) {
    e->count = specialize_instance_count() + 1;
    e->bodies = allocate(e->count, sizeof(*e->bodies));
    e->bodies[0].code = specialize_main_body();
    for (size_t i = 1; i < e->count; i++)
        e->bodies[i].code = specialize_instance_body(i - 1, &e->bodies[i].symbol);
    size_t maximum = 4;
    for (size_t i = 0; i < e->layout.count; i++)
        if (e->layout.items[i].symbol->usage == SYMBOL_CLASS && e->layout.items[i].size > maximum)
            maximum = e->layout.items[i].size;
    for (size_t i = 0; i < e->count; i++) {
        body_t *body = &e->bodies[i];
        body->size = layout_frame_size(&e->layout, body->symbol);
        body->scratch = allocate(body->code->count, sizeof(*body->scratch));
        for (size_t pc = 0; pc < body->code->count; pc++) {
            const rpn_step_t *step = &body->code->steps[pc];
            type_t type = {TYPE_ERROR, NULL};
            if (step->kind == RPN_SYMBOL) type = type_of_symbol(step->value.symbol);
            if (step->kind == RPN_ATTRIBUTE) type = type_of_attribute(step);
            if (object_type(type) || step->capture) {
                body->scratch[pc] = body->size;
                body->size += step->capture ? layout_class_size(&e->layout, step->capture->type_symbol) :
                    type.kind == TYPE_OBJECT ? layout_class_size(&e->layout, type.class_symbol) : maximum;
            }
        }
        if (body->size > MEMORY_LIMIT / 2)
            report_semantic_error(body->symbol ? body->symbol->declaration_line : 1, "Function frame exceeds implementation memory limit");
    }
}
static void write_body(emitter_t *e, body_t *body) {
    e->body = body;
    size_t capacity = body->code->count * 16 + 32;
    e->stack = allocate(capacity, sizeof(*e->stack));
    e->calls = allocate(capacity, sizeof(*e->calls));
    e->local_types = allocate(capacity, sizeof(*e->local_types));
    e->planning = true;
    e->locals = e->depth = e->call_depth = 0;
    emit_range(e, 0, body->code->count);
    size_t locals = e->locals;
    if (e->depth || e->call_depth) report_semantic_error(1, "Incomplete WebAssembly translation body");
    e->planning = false;
    if (!body->symbol) write_line(e, "(func $main (export \"main\") (local $frame i32) (local $result i32) (local $wide f64)");
    else write_line(e, "(func $fn_%zu (param $frame i32) (result %s) (local $result %s) (local $wide f64)",
        body_id(e), wasm_type(type_of_symbol(body->symbol)), wasm_type(type_of_symbol(body->symbol)));
    for (size_t i = 1; i <= locals; i++)
        write_line(e, "(local $tmp_%zu_%zu %s)", body_id(e), i, e->local_types[i] == TYPE_SINGLEF ? "f32" : "i32");
    write_line(e, ";; %s", body->symbol ? body->symbol->key : "main");
    if (!body->symbol) {
        write_line(e, "i32.const %zu\nlocal.set $frame", e->layout.strings_end);
        for (size_t i = 0; i < e->layout.count; i++) {
            const map_node_t *s = e->layout.items[i].symbol;
            if (s->usage == SYMBOL_VARIABLE && !s->owner && s->type_symbol)
                write_line(e, "local.get $frame\ni32.const %zu\ni32.add\ncall $init_%zu",
                    layout_offset(&e->layout, s), layout_id(&e->layout, s->type_symbol));
        }
    } else {
        size_t source = layout_id(&e->layout, layout_source(body->symbol));
        write_line(e, "global.get $active_%zu", source); fail_if(e, 6, body->symbol->declaration_line);
        write_line(e, "i32.const 1\nglobal.set $active_%zu", source);
    }
    write_line(e, "block $return");
    e->locals = e->depth = e->call_depth = 0;
    emit_range(e, 0, body->code->count);
    write_line(e, "end");
    if (body->symbol) {
        write_line(e, "i32.const 0\nglobal.set $active_%zu\nlocal.get $result", layout_id(&e->layout, layout_source(body->symbol)));
    }
    write_line(e, ")");
    free(e->stack); free(e->calls); free(e->local_types);
}
static void write_module(emitter_t *e) {
    write_line(e, "(module\n"
        "(import \"env\" \"printInteger\" (func $print_integer (param i32)))\n"
        "(import \"env\" \"printFloat\" (func $print_float (param f32)))\n"
        "(import \"env\" \"printString\" (func $print_string (param i32)))\n"
        "(import \"env\" \"runtimeFail\" (func $fail (param i32 i32)))\n"
        "(memory (export \"memory\") 256)\n(global $sp (mut i32) (i32.const %zu))",
        e->layout.strings_end + e->bodies[0].size);
    for (size_t i = 0; i < specialize_source_count(); i++) {
        map_node_t *symbol;
        specialize_source_body(i, &symbol);
        write_line(e, "(global $active_%zu (mut i32) (i32.const 0))", layout_id(&e->layout, symbol));
    }
    for (size_t i = 0; i < e->layout.count; i++) {
        const map_node_t *s = e->layout.items[i].symbol;
        if (s->usage != SYMBOL_LEXEME || strcmp(s->value, "CHAIN")) continue;
        fprintf(e->out, "(data (i32.const %zu) \"", e->layout.items[i].offset);
        for (const unsigned char *p = (const unsigned char *)s->key; *p; p++) fprintf(e->out, "\\%02x", *p);
        fputs("\\00\")\n", e->out);
    }
    write_helpers(e);
    for (size_t i = 0; i < e->count; i++) write_body(e, &e->bodies[i]);
    write_line(e, ")");
}
bool wasm_write(const char *path) {
    emitter_t e = {0};
    if (!layout_init(&e.layout)) { layout_destroy(&e.layout); return false; }
    prepare_bodies(&e);
    if (e.layout.strings_end + e.bodies[0].size > MEMORY_LIMIT)
        report_semantic_error(1, "Global data exceeds implementation memory limit");
    for (size_t i = 0; i < e.count; i++) types_check_body(e.bodies[i].code, e.bodies[i].symbol);
    char *temporary = malloc(strlen(path) + 16);
    if (!temporary) exit(1);
    sprintf(temporary, "%s.tmp.XXXXXX", path);
    int descriptor = -1;
    if (!global_errors) descriptor = mkstemp(temporary);
    if (descriptor >= 0) {
        e.out = fdopen(descriptor, "w");
        if (!e.out) close(descriptor);
    }
    bool success = e.out != NULL;
    if (success) {
        write_module(&e);
        success = !global_errors && !ferror(e.out);
        if (fclose(e.out)) success = false;
        if (success && rename(temporary, path)) success = false;
    }
    if (!success) {
        unlink(temporary);
        if (!global_errors) report_semantic_error(1, "Cannot write WebAssembly file '%s'", path);
    }
    for (size_t i = 0; i < e.count; i++) free(e.bodies[i].scratch);
    free(e.bodies); free(temporary); layout_destroy(&e.layout);
    return success;
}
