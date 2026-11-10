#include "../include/memory.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/wasm_emit.h"
#include "../include/layout.h"
#include "../include/specialize.h"
#include "../include/diagnostics.h"

#define MEMORY_LIMIT (16 * 1024 * 1024)

#define FRAME_STATIC_LINK_OFFSET 0
#define FRAME_RECEIVER_OFFSET 4
#define FRAME_HEADER_SIZE 8

#define INT16_MIN_WASM (-32768)
#define INT16_MAX_WASM 32767
#define INT16_SIGN_EXTEND_SHIFT 16
#define FLOAT32_MAX_ABS_STR "3.40282347e38"

typedef enum {
    WASM_ERR_FLOAT_OVERFLOW     = 1,
    WASM_ERR_INT_OVERFLOW       = 2,
    WASM_ERR_FRACTIONAL_TRUNC   = 3,
    WASM_ERR_FLOAT_TO_INT_RANGE = 4,
    WASM_ERR_INVALID_FIELD      = 5,
    WASM_ERR_RECURSIVE_CALL     = 6,
    WASM_ERR_OUT_OF_MEMORY      = 8
} wasm_runtime_error_t;

typedef struct {
    const rpn_t *code;
    map_node_t *symbol;
    size_t size;
    size_t *scratch;
    size_t *controls;
} body_t;

typedef struct {
    type_t type;
    size_t local;
    size_t tag;
} operand_t;

typedef struct {
    const map_node_t *symbol;
    size_t local;
} call_t;

typedef struct {
    FILE *out;
    layout_t layout;
    body_t *bodies;
    size_t count;
} wasm_module_t;

typedef struct {
    FILE *out;
    const wasm_module_t *module;
    body_t *body;
    operand_t *stack;
    size_t depth;
    call_t *calls;
    size_t call_depth;
    size_t locals;
    type_kind_t *local_types;
} body_emitter_t;

static void write_line(FILE *out, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);
    fputc('\n', out);
}

static size_t body_id(body_emitter_t *e) {
    return layout_id(&e->module->layout, e->body->symbol);
}

static const char *wasm_type(type_t type) {
    return type.kind == TYPE_SINGLEF ? "f32" : "i32";
}

static size_t new_local(body_emitter_t *e, type_t type, int line) {
    size_t id = ++e->locals;
    e->local_types[id] = type.kind;
    char key[96];
    snprintf(key, sizeof(key), "@tmp_%zu_%zu", body_id(e), id);
    map_node_t *s = map_put(symbol_table, key, "ID");
    symbol_set_usage(s, SYMBOL_AUXILIARY);
    s->owner = (map_node_t *)layout_source(e->body->symbol);
    s->declaration_line = line;
    s->type_symbol = (map_node_t *)type.class_symbol;
    if (!s->declared_type) {
        s->declared_type = strdup(type_name(type));
    }
    return id;
}

static size_t save_local(body_emitter_t *e, type_t type, int line) {
    size_t id = new_local(e, type, line);
    write_line(e->out, "local.set $tmp_%zu_%zu", body_id(e), id);
    return id;
}

static void get_local(body_emitter_t *e, size_t id) {
    write_line(e->out, "local.get $tmp_%zu_%zu", body_id(e), id);
}

static operand_t pop(body_emitter_t *e) {
    if (!e->depth) {
        report_semantic_error(1, "Invalid WebAssembly translation stack");
        return (operand_t){0};
    }
    return e->stack[--e->depth];
}

static void push(body_emitter_t *e, operand_t value) {
    e->stack[e->depth++] = value;
}

static void fail_if(FILE *out, int code, int line) {
    write_line(out, "if\ni32.const %d\ni32.const %d\ncall $fail\nunreachable\nend", code, line);
}

static type_t integer_type(void) {
    return (type_t){TYPE_INTEGER, NULL};
}

static void frame_address(body_emitter_t *e, const map_node_t *owner) {
    const map_node_t *current = layout_source(e->body->symbol);
    owner = layout_source(owner);
    write_line(e->out, "local.get $frame");
    while (current && current != owner) {
        write_line(e->out, "i32.load");
        current = layout_parent(current);
    }
    if (current != owner) {
        report_semantic_error(owner ? owner->declaration_line : 1, "Cannot locate lexical frame");
    }
}

static void receiver_address(body_emitter_t *e) {
    const map_node_t *owner = layout_source(e->body->symbol);
    while (owner && owner->usage != SYMBOL_METHOD) {
        owner = layout_parent(owner);
    }
    if (!owner) {
        report_semantic_error(1, "Missing method receiver");
        write_line(e->out, "i32.const 0");
        return;
    }
    frame_address(e, owner);
    write_line(e->out, "i32.load offset=%d", FRAME_RECEIVER_OFFSET);
}

static void symbol_address(body_emitter_t *e, const map_node_t *symbol, int line) {
    if (symbol->usage == SYMBOL_ATTRIBUTE) {
        receiver_address(e);
        write_line(e->out, "i32.const %zu\ni32.const %d\ncall $field", layout_id(&e->module->layout, symbol), line);
    } else {
        frame_address(e, symbol->owner);
        write_line(e->out, "i32.const %zu\ni32.add", layout_offset(&e->module->layout, symbol));
    }
}

static bool object_type(type_t type) {
    if (type.kind == TYPE_OBJECT) {
        return true;
    }
    if (type.kind != TYPE_CANDIDATES) {
        return false;
    }
    for (size_t i = 0; i < type.class_symbol->class_info->effective_attributes.count; i++) {
        const map_node_t *attr = type.class_symbol->class_info->effective_attributes.items[i];
        if (type_of_symbol(attr).kind != TYPE_OBJECT) {
            return false;
        }
    }
    return true;
}

static size_t snapshot(body_emitter_t *e, size_t source, size_t pc) {
    write_line(e->out, "local.get $frame\ni32.const %zu\ni32.add", e->body->scratch[pc]);
    size_t result = save_local(e, integer_type(), e->body->code->steps[pc].source_line);
    get_local(e, result);
    get_local(e, source);
    get_local(e, source);
    write_line(e->out, "call $object_size\nmemory.copy");
    return result;
}

static void materialize(body_emitter_t *e, size_t object, const map_node_t *capture, size_t offset);

static void emit_float_const(body_emitter_t *e, const char *key) {
    char *text = strdup(key);
    for (char *p = text; *p; p++) {
        if (*p == 's') {
            *p = 'e';
        }
    }
    write_line(e->out, "f32.const %s", text);
    free(text);
}

static operand_t read_symbol(body_emitter_t *e, const map_node_t *s, size_t pc) {
    int line = e->body->code->steps[pc].source_line;
    type_t type = type_of_symbol(s);
    if (s->usage == SYMBOL_LEXEME) {
        if (type.kind == TYPE_CHAIN) {
            write_line(e->out, "i32.const %zu", layout_offset(&e->module->layout, s));
        } else if (type.kind == TYPE_INTEGER) {
            write_line(e->out, "i32.const %ld", strtol(s->key, NULL, 10));
        } else {
            emit_float_const(e, s->key);
        }
    } else {
        symbol_address(e, s, line);
        if (type.kind != TYPE_OBJECT) {
            write_line(e->out, "%s.load", wasm_type(type));
        }
    }
    size_t local = save_local(e, type, line);
    if (type.kind == TYPE_OBJECT) {
        local = snapshot(e, local, pc);
    }
    return (operand_t){type, local, 0};
}

static void attribute(body_emitter_t *e, const rpn_step_t *step, size_t pc) {
    type_t type = type_of_attribute(step);
    operand_t index = read_symbol(e, step->index, pc);
    symbol_address(e, step->receiver, step->source_line);
    size_t object = save_local(e, integer_type(), step->source_line);
    if (step->capture) {
        object = snapshot(e, object, pc);
        materialize(e, object, step->capture, 0);
    }
    get_local(e, object);
    get_local(e, index.local);
    write_line(e->out, "i32.const %d\ncall $position", step->source_line);
    size_t address = save_local(e, integer_type(), step->source_line);
    bool destination = step->kind == RPN_ATTRIBUTE_DESTINATION;
    size_t local = address;
    size_t tag = 0;
    if (!destination) {
        if (object_type(type)) {
            local = snapshot(e, address, pc);
        } else {
            if (type.kind == TYPE_CANDIDATES) {
                get_local(e, object);
                get_local(e, index.local);
                write_line(e->out, "i32.const %d\ncall $field_type", step->source_line);
                tag = save_local(e, integer_type(), step->source_line);
            }
            get_local(e, address);
            write_line(e->out, "%s.load", wasm_type(type));
            local = save_local(e, type, step->source_line);
        }
    }
    push(e, (operand_t){type, local, tag});
}

static body_t *find_body(body_emitter_t *e, const map_node_t *symbol) {
    for (size_t i = 1; i < e->module->count; i++) {
        if (e->module->bodies[i].symbol == symbol) {
            return &e->module->bodies[i];
        }
    }
    report_semantic_error(symbol->declaration_line, "Missing executable body '%s'", symbol->key);
    return &e->module->bodies[0];
}

static void initialize_frame(body_emitter_t *e, size_t local, const map_node_t *function) {
    const map_node_t *source = layout_source(function);
    for (size_t i = 0; i < e->module->layout.count; i++) {
        const map_node_t *s = e->module->layout.items[i].symbol;
        if ((s->usage == SYMBOL_VARIABLE || s->usage == SYMBOL_PARAMETER) && s->owner == source && s->type_symbol) {
            get_local(e, local);
            write_line(e->out, "i32.const %zu\ni32.add\ncall $init_%zu",
                       layout_offset(&e->module->layout, s), layout_id(&e->module->layout, s->type_symbol));
        }
    }
}

static void begin_call(body_emitter_t *e, const rpn_step_t *step) {
    body_t *body = find_body(e, step->value.symbol);
    write_line(e->out, "i32.const %zu\ni32.const %d\ncall $allocate", body->size, step->source_line);
    size_t frame = save_local(e, integer_type(), step->source_line);
    get_local(e, frame);
    frame_address(e, layout_parent(step->value.symbol));
    write_line(e->out, "i32.store");
    const map_node_t *source = layout_source(step->value.symbol);
    if (source->usage == SYMBOL_METHOD) {
        get_local(e, frame);
        if (step->receiver) {
            symbol_address(e, step->receiver, step->source_line);
        } else {
            receiver_address(e);
        }
        write_line(e->out, "i32.store offset=%d", FRAME_RECEIVER_OFFSET);
    }
    initialize_frame(e, frame, step->value.symbol);
    e->calls[e->call_depth++] = (call_t){step->value.symbol, frame};
}

static void call_function(body_emitter_t *e, const rpn_step_t *step) {
    call_t call = e->calls[--e->call_depth];
    get_local(e, call.local);
    write_line(e->out, "call $fn_%zu", layout_id(&e->module->layout, step->value.symbol));
    type_t type = type_of_symbol(step->value.symbol);
    size_t result = save_local(e, type, step->source_line);
    get_local(e, call.local);
    write_line(e->out, "global.set $sp");
    push(e, (operand_t){type, result, 0});
}

static void materialize(body_emitter_t *e, size_t object, const map_node_t *capture, size_t offset) {
    const symbol_capture_info_t *info = capture->capture_info;
    if (info == NULL) {
        return;
    }
    const map_node_t *kind = capture->type_symbol;
    for (size_t i = 0; i < kind->class_info->effective_attributes.count; i++) {
        const map_node_t *field = kind->class_info->effective_attributes.items[i];
        size_t field_offset = offset + layout_field_offset(&e->module->layout, kind, i);
        for (size_t j = 0; j < info->comptime_bindings.count; j++) {
            if (info->comptime_bindings.items[j] == field) {
                const map_node_t *constant = info->comptime_values.items[j];
                get_local(e, object);
                write_line(e->out, "i32.const %zu\ni32.add", field_offset);
                if (type_of_symbol(field).kind == TYPE_INTEGER) {
                    write_line(e->out, "i32.const %ld", strtol(constant->key, NULL, 10));
                } else {
                    emit_float_const(e, constant->key);
                }
                write_line(e->out, "%s.store", wasm_type(type_of_symbol(field)));
            }
        }
        for (size_t j = 0; j < info->object_fields.count; j++) {
            if (info->object_fields.items[j] == field) {
                materialize(e, object, info->object_values.items[j], field_offset);
            }
        }
    }
}

static void emit_float_add_checked(body_emitter_t *e, operand_t left, operand_t right, int line) {
    get_local(e, left.local);
    write_line(e->out, "f64.promote_f32");
    get_local(e, right.local);
    write_line(e->out, "f64.promote_f32\nf64.add\nlocal.tee $wide\nf64.abs\nf64.const " FLOAT32_MAX_ABS_STR "\nf64.ge");
    fail_if(e->out, WASM_ERR_FLOAT_OVERFLOW, line);
    write_line(e->out, "local.get $wide\nf32.demote_f64");
}

static void emit_int16_overflow_check(body_emitter_t *e, size_t result, int line) {
    get_local(e, result);
    write_line(e->out, "i32.const %d\ni32.lt_s", INT16_MIN_WASM);
    get_local(e, result);
    write_line(e->out, "i32.const %d\ni32.gt_s\ni32.or", INT16_MAX_WASM);
    fail_if(e->out, WASM_ERR_INT_OVERFLOW, line);
}

static void emit_int16_sign_extend(body_emitter_t *e, size_t result) {
    get_local(e, result);
    write_line(e->out, "i32.const %d\ni32.shl\ni32.const %d\ni32.shr_s\nlocal.set $tmp_%zu_%zu",
               INT16_SIGN_EXTEND_SHIFT, INT16_SIGN_EXTEND_SHIFT, body_id(e), result);
}

static void binary(body_emitter_t *e, rpn_operator_t op, int line) {
    operand_t right = pop(e);
    operand_t left = pop(e);
    bool floating = left.type.kind == TYPE_SINGLEF;
    static const char *integer_ops[] = {"add", "sub", "mul", "div_s", "", "lt_s", "gt_s", "le_s", "ge_s", "eq", "ne"};
    static const char *float_ops[] = {"add", "sub", "mul", "div", "", "lt", "gt", "le", "ge", "eq", "ne"};
    type_t result_type = op >= RPN_LESS ? integer_type() : left.type;
    if (floating && op == RPN_ADD) {
        emit_float_add_checked(e, left, right, line);
    } else {
        get_local(e, left.local);
        get_local(e, right.local);
        write_line(e->out, "%s.%s", floating ? "f32" : "i32", floating ? float_ops[op] : integer_ops[op]);
    }
    size_t result = save_local(e, result_type, line);
    if (!floating && op == RPN_MULTIPLY) {
        emit_int16_overflow_check(e, result, line);
    }
    if (floating && op == RPN_ADD) {
        get_local(e, result);
        write_line(e->out, "f64.promote_f32\nf64.abs\nf64.const " FLOAT32_MAX_ABS_STR "\nf64.ge");
        fail_if(e->out, WASM_ERR_FLOAT_OVERFLOW, line);
    }
    if (!floating && op < RPN_ASSIGN) {
        emit_int16_sign_extend(e, result);
    }
    push(e, (operand_t){result_type, result, 0});
}

static void convert_integer(body_emitter_t *e, int line) {
    operand_t value = pop(e);
    get_local(e, value.local);
    get_local(e, value.local);
    write_line(e->out, "f32.ne");
    get_local(e, value.local);
    write_line(e->out, "f32.const %d\nf32.lt\ni32.or", INT16_MIN_WASM);
    get_local(e, value.local);
    write_line(e->out, "f32.const %d\nf32.gt\ni32.or", INT16_MAX_WASM);
    fail_if(e->out, WASM_ERR_FLOAT_TO_INT_RANGE, line);
    get_local(e, value.local);
    get_local(e, value.local);
    write_line(e->out, "f32.trunc\nf32.ne");
    fail_if(e->out, WASM_ERR_FRACTIONAL_TRUNC, line);
    get_local(e, value.local);
    write_line(e->out, "i32.trunc_f32_s");
    push(e, (operand_t){integer_type(), save_local(e, integer_type(), line), 0});
}

static void output(body_emitter_t *e) {
    operand_t value = pop(e);
    if (value.type.kind == TYPE_CANDIDATES) {
        get_local(e, value.tag);
        write_line(e->out, "i32.const %d\ni32.eq\nif", TYPE_INTEGER);
        get_local(e, value.local);
        write_line(e->out, "call $print_integer\nelse");
        get_local(e, value.local);
        write_line(e->out, "f32.reinterpret_i32\ncall $print_float\nend");
    } else {
        get_local(e, value.local);
        write_line(e->out, "call $print_%s",
                   value.type.kind == TYPE_SINGLEF ? "float" : value.type.kind == TYPE_CHAIN ? "chain" : "integer");
    }
}

static void emit_operator(body_emitter_t *e, const rpn_step_t *step) {
    rpn_operator_t op = step->value.operator;
    if (op <= RPN_NOT_EQUAL && op != RPN_ASSIGN) {
        binary(e, op, step->source_line);
    } else if (op == RPN_ASSIGN) {
        operand_t destination = pop(e);
        operand_t value = pop(e);
        get_local(e, destination.local);
        get_local(e, value.local);
        if (object_type(value.type)) {
            write_line(e->out, "call $copy_object");
        } else {
            write_line(e->out, "%s.store", wasm_type(value.type));
        }
    } else if (op == RPN_DUPLICATE) {
        operand_t value = pop(e);
        push(e, value);
        push(e, value);
    } else if (op == RPN_DROP) {
        pop(e);
    } else if (op == RPN_TO_INTEGER) {
        convert_integer(e, step->source_line);
    } else if (op == RPN_OUTPUT) {
        output(e);
    } else if (op == RPN_RETURN) {
        operand_t value = pop(e);
        get_local(e, value.local);
        write_line(e->out, "local.set $result\nbr $return");
    } else {
        report_semantic_error(step->source_line, "Unexpected branch outside structured region");
    }
}

static void emit_range(body_emitter_t *e, size_t begin, size_t end);

static size_t emit_control(body_emitter_t *e, size_t begin, size_t target) {
    rpn_control_t control = rpn_describe_control(e->body->code, begin, target);
    size_t false_start = control.false_start;
    size_t body_end = control.body_end;
    size_t finish = control.end;
    bool has_else = control.has_else;
    if (control.loop) {
        write_line(e->out, "block $exit_%zu\nloop $loop_%zu", target, target);
    }
    emit_range(e, begin, target);
    operand_t condition = pop(e);
    get_local(e, condition.local);
    if (control.loop) {
        write_line(e->out, "i32.eqz\nbr_if $exit_%zu", target);
        emit_range(e, target + 2, body_end);
        write_line(e->out, "br $loop_%zu\nend\nend", target);
    } else {
        write_line(e->out, "if");
        emit_range(e, target + 2, body_end);
        if (has_else) {
            write_line(e->out, "else");
            emit_range(e, false_start, finish);
        }
        write_line(e->out, "end");
    }
    return finish;
}

static void emit_step(body_emitter_t *e, size_t pc) {
    const rpn_step_t *step = &e->body->code->steps[pc];
    write_line(e->out, ";; RPN %zu, source line %d", pc + 1, step->source_line);
    switch (step->kind) {
    case RPN_SYMBOL:
        push(e, read_symbol(e, step->value.symbol, pc));
        break;
    case RPN_DESTINATION:
    case RPN_ARGUMENT_DESTINATION: {
        if (step->kind == RPN_ARGUMENT_DESTINATION) {
            get_local(e, e->calls[e->call_depth - 1].local);
            write_line(e->out, "i32.const %zu\ni32.add", layout_offset(&e->module->layout, step->value.symbol));
        } else {
            symbol_address(e, step->value.symbol, step->source_line);
        }
        push(e, (operand_t){type_of_symbol(step->value.symbol), save_local(e, integer_type(), step->source_line), 0});
        break;
    }
    case RPN_ATTRIBUTE:
    case RPN_ATTRIBUTE_DESTINATION:
        attribute(e, step, pc);
        break;
    case RPN_INDEX_CHECK: {
        operand_t index = read_symbol(e, step->index, pc);
        symbol_address(e, step->receiver, step->source_line);
        get_local(e, index.local);
        write_line(e->out, "i32.const %d\ncall $position\ndrop", step->source_line);
        break;
    }
    case RPN_CALL_FRAME:
        begin_call(e, step);
        break;
    case RPN_CALL:
        call_function(e, step);
        break;
    case RPN_OPERATOR:
        emit_operator(e, step);
        break;
    case RPN_OBJECT_VALUE:
        if (e->depth) {
            materialize(e, e->stack[e->depth - 1].local, step->value.symbol, 0);
        }
        break;
    case RPN_TARGET:
        report_semantic_error(step->source_line, "Unstructured jump target in WebAssembly generation");
        break;
    }
}

static void emit_range(body_emitter_t *e, size_t begin, size_t end) {
    for (size_t pc = begin; pc < end; pc++) {
        size_t target = e->body->controls[pc] ? e->body->controls[pc] - 1 : end;
        if (target < end) {
            pc = emit_control(e, pc, target) - 1;
        } else {
            emit_step(e, pc);
        }
    }
}

static void write_memory_helpers(wasm_module_t *mod) {
    write_line(mod->out, "(func $allocate (param $size i32) (param $line i32) (result i32) (local $old i32)\n"
                         "global.get $sp\nlocal.tee $old\nlocal.get $size\ni32.add\ni32.const %d\ni32.gt_u\n"
                         "if\ni32.const %d\nlocal.get $line\ncall $fail\nunreachable\nend\n"
                         "local.get $old\ni32.const 0\nlocal.get $size\nmemory.fill\n"
                         "local.get $old\nlocal.get $size\ni32.add\nglobal.set $sp\nlocal.get $old)",
               MEMORY_LIMIT, WASM_ERR_OUT_OF_MEMORY);
}

static void write_field_helper(wasm_module_t *mod, const char *name, bool index_by_symbol_id, bool emit_type_kind) {
    write_line(mod->out, "(func $%s (param $object i32) (param $index i32) (param $line i32) (result i32)", name);
    for (size_t i = 0; i < mod->layout.count; i++) {
        const map_node_t *kind = mod->layout.items[i].symbol;
        if (kind->usage != SYMBOL_CLASS) {
            continue;
        }
        write_line(mod->out, "local.get $object\ni32.load\ni32.const %zu\ni32.eq\nif", mod->layout.items[i].id);
        for (size_t j = 0; j < kind->class_info->effective_attributes.count; j++) {
            const map_node_t *field = kind->class_info->effective_attributes.items[j];
            write_line(mod->out, "local.get $index\ni32.const %zu\ni32.eq\nif",
                       index_by_symbol_id ? layout_id(&mod->layout, field) : j);
            if (emit_type_kind) {
                write_line(mod->out, "i32.const %d", type_of_symbol(field).kind);
            } else {
                write_line(mod->out, "local.get $object\ni32.const %zu\ni32.add", layout_field_offset(&mod->layout, kind, j));
            }
            write_line(mod->out, "return\nend");
        }
        write_line(mod->out, "end");
    }
    write_line(mod->out, "i32.const %d\nlocal.get $line\ncall $fail\nunreachable)", WASM_ERR_INVALID_FIELD);
}

static void write_object_helpers(wasm_module_t *mod) {
    write_line(mod->out, "(func $object_size (param $object i32) (result i32)");
    for (size_t i = 0; i < mod->layout.count; i++) {
        const layout_entry_t *item = &mod->layout.items[i];
        if (item->symbol->usage != SYMBOL_CLASS) {
            continue;
        }
        write_line(mod->out, "local.get $object\ni32.load\ni32.const %zu\ni32.eq\nif\ni32.const %zu\nreturn\nend", item->id, item->size);
    }
    write_line(mod->out, "unreachable)");
    for (size_t i = 0; i < mod->layout.count; i++) {
        const layout_entry_t *item = &mod->layout.items[i];
        const map_node_t *kind = item->symbol;
        if (kind->usage != SYMBOL_CLASS) {
            continue;
        }
        write_line(mod->out, "(func $init_%zu (param $object i32)\nlocal.get $object\ni32.const %zu\ni32.store", item->id, item->id);
        for (size_t j = 0; j < kind->class_info->effective_attributes.count; j++) {
            const map_node_t *field = kind->class_info->effective_attributes.items[j];
            if (field->type_symbol) {
                write_line(mod->out, "local.get $object\ni32.const %zu\ni32.add\ncall $init_%zu",
                           layout_field_offset(&mod->layout, kind, j), layout_id(&mod->layout, field->type_symbol));
            }
        }
        write_line(mod->out, ")");
    }
    write_field_helper(mod, "position", false, false);
    write_field_helper(mod, "field", true, false);
    write_field_helper(mod, "field_type", false, true);
}

static void write_copy_helper(wasm_module_t *mod) {
    write_line(mod->out, "(func $copy_object (param $destination i32) (param $source i32)");
    for (size_t i = 0; i < mod->layout.count; i++) {
        const map_node_t *dst = mod->layout.items[i].symbol;
        if (dst->usage != SYMBOL_CLASS) {
            continue;
        }
        for (size_t j = 0; j < mod->layout.count; j++) {
            const map_node_t *src = mod->layout.items[j].symbol;
            if (src->usage != SYMBOL_CLASS || !types_compatible((type_t){TYPE_OBJECT, dst}, (type_t){TYPE_OBJECT, src})) {
                continue;
            }
            write_line(mod->out, "local.get $destination\ni32.load\ni32.const %zu\ni32.eq\nlocal.get $source\ni32.load\ni32.const %zu\ni32.eq\ni32.and\nif",
                       layout_id(&mod->layout, dst), layout_id(&mod->layout, src));
            for (size_t k = 0; k < dst->class_info->effective_attributes.count; k++) {
                const map_node_t *field = dst->class_info->effective_attributes.items[k];
                write_line(mod->out, "local.get $destination\ni32.const %zu\ni32.add\nlocal.get $source\ni32.const %zu\ni32.add",
                           layout_field_offset(&mod->layout, dst, k), layout_field_offset(&mod->layout, src, k));
                write_line(mod->out, field->type_symbol ? "call $copy_object" : "i32.load\ni32.store");
            }
            write_line(mod->out, "return\nend");
        }
    }
    write_line(mod->out, "unreachable)");
}

static void prepare_bodies(wasm_module_t *mod) {
    mod->count = specialize_instance_count() + 1;
    mod->bodies = memory_zero(mod->count, sizeof(*mod->bodies));
    mod->bodies[0].code = specialize_main_body();
    for (size_t i = 1; i < mod->count; i++) {
        mod->bodies[i].code = specialize_instance_body(i - 1, &mod->bodies[i].symbol);
    }
    size_t maximum = 4;
    for (size_t i = 0; i < mod->layout.count; i++) {
        if (mod->layout.items[i].symbol->usage == SYMBOL_CLASS && mod->layout.items[i].size > maximum) {
            maximum = mod->layout.items[i].size;
        }
    }
    for (size_t i = 0; i < mod->count; i++) {
        body_t *body = &mod->bodies[i];
        body->controls = rpn_index_controls(body->code);
        body->size = layout_frame_size(&mod->layout, body->symbol);
        body->scratch = memory_zero(body->code->count, sizeof(*body->scratch));
        for (size_t pc = 0; pc < body->code->count; pc++) {
            const rpn_step_t *step = &body->code->steps[pc];
            type_t type = {TYPE_ERROR, NULL};
            if (step->kind == RPN_SYMBOL) {
                type = type_of_symbol(step->value.symbol);
            }
            if (step->kind == RPN_ATTRIBUTE) {
                type = type_of_attribute(step);
            }
            if (object_type(type) || step->capture) {
                body->scratch[pc] = body->size;
                body->size += step->capture ? layout_class_size(&mod->layout, step->capture->type_symbol) :
                    type.kind == TYPE_OBJECT ? layout_class_size(&mod->layout, type.class_symbol) : maximum;
            }
        }
        if (body->size > MEMORY_LIMIT / 2) {
            report_semantic_error(body->symbol ? body->symbol->declaration_line : 1, "Function frame exceeds implementation memory limit");
        }
    }
}

static void write_body_prologue(wasm_module_t *mod, const body_t *body) {
    write_line(mod->out, ";; %s", body->symbol ? body->symbol->key : "main");
    if (!body->symbol) {
        write_line(mod->out, "i32.const %zu\nlocal.set $frame", mod->layout.chains_end);
        for (size_t i = 0; i < mod->layout.count; i++) {
            const map_node_t *s = mod->layout.items[i].symbol;
            if (s->usage == SYMBOL_VARIABLE && !s->owner && s->type_symbol) {
                write_line(mod->out, "local.get $frame\ni32.const %zu\ni32.add\ncall $init_%zu",
                           layout_offset(&mod->layout, s), layout_id(&mod->layout, s->type_symbol));
            }
        }
    } else {
        size_t source = layout_id(&mod->layout, layout_source(body->symbol));
        write_line(mod->out, "global.get $active_%zu", source);
        fail_if(mod->out, WASM_ERR_RECURSIVE_CALL, body->symbol->declaration_line);
        write_line(mod->out, "i32.const 1\nglobal.set $active_%zu", source);
    }
}

static void write_body_epilogue(wasm_module_t *mod, const body_t *body) {
    if (body->symbol) {
        write_line(mod->out, "i32.const 0\nglobal.set $active_%zu\nlocal.get $result",
                   layout_id(&mod->layout, layout_source(body->symbol)));
    }
    write_line(mod->out, ")");
}

static void write_body(wasm_module_t *mod, body_t *body) {
    size_t capacity = body->code->count * 16 + 32;
    char *instructions_buf = NULL;
    size_t instructions_len = 0;
    FILE *body_out = open_memstream(&instructions_buf, &instructions_len);
    if (!body_out) {
        exit(1);
    }

    body_emitter_t e = {
        .out = body_out,
        .module = mod,
        .body = body,
        .stack = memory_zero(capacity, sizeof(operand_t)),
        .depth = 0,
        .calls = memory_zero(capacity, sizeof(call_t)),
        .call_depth = 0,
        .locals = 0,
        .local_types = memory_zero(capacity, sizeof(type_kind_t))
    };

    emit_range(&e, 0, body->code->count);

    fclose(body_out);

    if (e.depth || e.call_depth) {
        report_semantic_error(1, "Incomplete WebAssembly translation body");
    }

    size_t locals = e.locals;
    if (!body->symbol) {
        write_line(mod->out, "(func $main (export \"main\") (local $frame i32) (local $result i32) (local $wide f64)");
    } else {
        write_line(mod->out, "(func $fn_%zu (param $frame i32) (result %s) (local $result %s) (local $wide f64)",
                   layout_id(&mod->layout, body->symbol), wasm_type(type_of_symbol(body->symbol)), wasm_type(type_of_symbol(body->symbol)));
    }
    for (size_t i = 1; i <= locals; i++) {
        write_line(mod->out, "(local $tmp_%zu_%zu %s)",
                   layout_id(&mod->layout, body->symbol), i, e.local_types[i] == TYPE_SINGLEF ? "f32" : "i32");
    }
    write_body_prologue(mod, body);
    write_line(mod->out, "block $return");
    if (instructions_len > 0) {
        fwrite(instructions_buf, 1, instructions_len, mod->out);
    }
    write_line(mod->out, "end");
    write_body_epilogue(mod, body);

    free(instructions_buf);
    free(e.stack);
    free(e.calls);
    free(e.local_types);
}

static void write_module(wasm_module_t *mod) {
    write_line(mod->out, "(module\n"
                         "(import \"env\" \"printInteger\" (func $print_integer (param i32)))\n"
                         "(import \"env\" \"printFloat\" (func $print_float (param f32)))\n"
                         "(import \"env\" \"printString\" (func $print_chain (param i32)))\n"
                         "(import \"env\" \"runtimeFail\" (func $fail (param i32 i32)))\n"
                         "(memory (export \"memory\") 256)\n(global $sp (mut i32) (i32.const %zu))",
               mod->layout.chains_end + mod->bodies[0].size);
    for (size_t i = 0; i < specialize_source_count(); i++) {
        map_node_t *symbol;
        specialize_source_body(i, &symbol);
        write_line(mod->out, "(global $active_%zu (mut i32) (i32.const 0))", layout_id(&mod->layout, symbol));
    }
    for (size_t i = 0; i < mod->layout.count; i++) {
        const map_node_t *s = mod->layout.items[i].symbol;
        if (s->usage != SYMBOL_LEXEME || strcmp(s->value, "CHAIN")) {
            continue;
        }
        fprintf(mod->out, "(data (i32.const %zu) \"", mod->layout.items[i].offset);
        for (const unsigned char *p = (const unsigned char *)s->key; *p; p++) {
            fprintf(mod->out, "\\%02x", *p);
        }
        fputs("\\00\")\n", mod->out);
    }
    write_memory_helpers(mod);
    write_object_helpers(mod);
    write_copy_helper(mod);
    for (size_t i = 0; i < mod->count; i++) {
        write_body(mod, &mod->bodies[i]);
    }
    write_line(mod->out, ")");
}

bool wasm_write(const char *path) {
    wasm_module_t mod = {0};
    if (!layout_init(&mod.layout)) {
        layout_destroy(&mod.layout);
        return false;
    }
    prepare_bodies(&mod);
    if (mod.layout.chains_end + mod.bodies[0].size > MEMORY_LIMIT) {
        report_semantic_error(1, "Global data exceeds implementation memory limit");
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
