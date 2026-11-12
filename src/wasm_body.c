#include "../include/wasm_internal.h"
#include "../include/memory.h"
#include "../include/specialize.h"
#include "../include/diagnostics.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/*
 * Traductor RPN -> WebAssembly (ordenado de arriba hacia abajo):
 *
 * Como WebAssembly necesita releer valores intermedios varias veces (por ejemplo,
 * al copiar objetos, chequear overflows o liberar el marco tras una llamada),
 * cada resultado intermedio se guarda en una variable local $tmp_<body_id>_<id>.
 * La pila en C (e->stack) almacena descriptores de esos temporales y sus tipos.
 */

typedef struct {
    type_t type;
    size_t temp_id;
    size_t tag_temp_id;
} operand_t;

typedef struct {
    const map_node_t *symbol;
    size_t frame_temp_id;
} call_t;

typedef struct {
    FILE *out;
    int indent;
    const wasm_module_t *module;
    body_t *body;
    operand_t *stack;
    size_t depth;
    call_t *calls;
    size_t call_depth;
    size_t temps;
    type_kind_t *temp_types;
} body_emitter_t;

static void write_body_prologue(wasm_module_t *mod, const body_t *body);
static void write_body_epilogue(wasm_module_t *mod, const body_t *body);
static void emit_range(body_emitter_t *e, size_t begin, size_t end);
static size_t emit_control(body_emitter_t *e, size_t begin, size_t target);
static void emit_step(body_emitter_t *e, size_t pc);

static void emit_symbol_step(body_emitter_t *e, const map_node_t *s, size_t pc);
static void emit_destination_step(body_emitter_t *e, const rpn_step_t *step);
static void emit_attr_step(body_emitter_t *e, const rpn_step_t *step, size_t pc);
static size_t emit_attr_address(body_emitter_t *e, const rpn_step_t *step, size_t pc, size_t *out_object, size_t *out_index);
static void emit_index_check_step(body_emitter_t *e, const rpn_step_t *step, size_t pc);
static void emit_call_frame(body_emitter_t *e, const rpn_step_t *step);
static void emit_call(body_emitter_t *e, const rpn_step_t *step);
static void emit_operator(body_emitter_t *e, const rpn_step_t *step);
static void emit_materialize(body_emitter_t *e, size_t object_temp, const map_node_t *capture, layout_size_t offset);

static void emit_binary(body_emitter_t *e, rpn_operator_t op, int line);
static void emit_assign(body_emitter_t *e);
static void emit_convert_integer(body_emitter_t *e, int line);
static void emit_output(body_emitter_t *e);

static void emit_frame_address(body_emitter_t *e, const map_node_t *owner);
static void emit_receiver_address(body_emitter_t *e);
static void emit_symbol_address(body_emitter_t *e, const map_node_t *symbol, int line);
static void emit_initialize_frame(body_emitter_t *e, size_t frame_temp, const map_node_t *function);
static size_t emit_snapshot(body_emitter_t *e, size_t source_temp, size_t pc);
static void emit_float_const(body_emitter_t *e, const char *key);
static void emit_fail_if(FILE *out, int indent, int code, int line);
static void emit_float_add_checked(body_emitter_t *e, operand_t left, operand_t right, int line);
static void emit_int16_overflow_check(body_emitter_t *e, size_t result_temp, int line);
static void emit_int16_sign_extend(body_emitter_t *e, size_t result_temp);

static size_t alloc_temp(body_emitter_t *e, type_t type, int line);
static size_t emit_pop_to_temp(body_emitter_t *e, type_t type, int line);
static void emit_push_temp(body_emitter_t *e, size_t temp_id);
static operand_t pop_operand(body_emitter_t *e);
static void push_operand(body_emitter_t *e, operand_t value);

static size_t body_id(const body_emitter_t *e);
static const char *wasm_type(type_t type);
static type_t integer_type(void);
static bool object_type(type_t type);
static body_t *find_body(body_emitter_t *e, const map_node_t *symbol);
static bool has_control_jump(const body_t *body, size_t pc, size_t end, size_t *target);

/* --- Preparación y estructura de funciones --- */

void wasm_prepare_bodies(wasm_module_t *mod) {
    mod->count = memory_add(specialize_instance_count(), 1);
    mod->bodies = memory_zero(mod->count, sizeof(*mod->bodies));
    mod->bodies[0].code = specialize_main_body();
    for (size_t i = 1; i < mod->count; i++) {
        mod->bodies[i].code = specialize_instance_body(i - 1, &mod->bodies[i].symbol);
    }

    layout_size_t maximum = 4;
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
            } else if (step->kind == RPN_ATTRIBUTE) {
                type = type_of_attribute(step);
            }
            if (object_type(type) || step->capture) {
                body->scratch[pc] = body->size;
                layout_size_t size = step->capture
                    ? layout_class_size(&mod->layout, step->capture->type_symbol)
                    : (type.kind == TYPE_OBJECT ? layout_class_size(&mod->layout, type.class_symbol) : maximum);
                body->size = layout_add(body->size, size, step->source_line);
                if (!body->size) {
                    return;
                }
            }
        }
    }
}

void wasm_write_body(wasm_module_t *mod, body_t *body) {
    size_t capacity = memory_add(memory_multiply(body->code->count, 16), 32);
    char *instructions_buf = NULL;
    size_t instructions_len = 0;
    FILE *body_out = open_memstream(&instructions_buf, &instructions_len);
    if (!body_out) {
        exit(1);
    }

    body_emitter_t e = {
        .out = body_out,
        .indent = 3,
        .module = mod,
        .body = body,
        .stack = memory_zero(capacity, sizeof(operand_t)),
        .depth = 0,
        .calls = memory_zero(capacity, sizeof(call_t)),
        .call_depth = 0,
        .temps = 0,
        .temp_types = memory_zero(capacity, sizeof(type_kind_t))
    };

    emit_range(&e, 0, body->code->count);
    fclose(body_out);

    if (e.depth || e.call_depth) {
        report_semantic_error(1, "Incomplete WebAssembly translation body");
    }

    size_t id = layout_id(&mod->layout, body->symbol);
    if (!body->symbol) {
        wasm_emit_line(mod->out, 1, "(func $main (export \"main\") (local $frame i32) (local $result i32) (local $wide f64)");
    } else {
        const char *ret_type = wasm_type(type_of_symbol(body->symbol));
        wasm_emit_line(mod->out, 1, "(func $fn_%zu (param $frame i32) (result %s) (local $result %s) (local $wide f64)",
                       id, ret_type, ret_type);
    }

    for (size_t i = 1; i <= e.temps; i++) {
        wasm_emit_line(mod->out, 2, "(local $tmp_%zu_%zu %s)",
                       id, i, e.temp_types[i] == TYPE_SINGLEF ? "f32" : "i32");
    }

    write_body_prologue(mod, body);
    wasm_emit_line(mod->out, 2, "block $return");
    if (instructions_len > 0) {
        fwrite(instructions_buf, 1, instructions_len, mod->out);
    }
    wasm_emit_line(mod->out, 2, "end");
    write_body_epilogue(mod, body);

    free(instructions_buf);
    free(e.stack);
    free(e.calls);
    free(e.temp_types);
}

static void write_body_prologue(wasm_module_t *mod, const body_t *body) {
    wasm_emit_line(mod->out, 2, ";; %s", body->symbol ? body->symbol->key : "main");
    if (!body->symbol) {
        wasm_emit_line(mod->out, 2, "i64.const %" PRIu64, mod->layout.chains_end);
        wasm_emit_line(mod->out, 2, "global.set $sp");
        wasm_emit_line(mod->out, 2, "i32.const %" PRIu64, body->size);
        wasm_emit_line(mod->out, 2, "i32.const 1");
        wasm_emit_line(mod->out, 2, "call $allocate");
        wasm_emit_line(mod->out, 2, "local.set $frame");
        for (size_t i = 0; i < mod->layout.count; i++) {
            const map_node_t *s = mod->layout.items[i].symbol;
            if (s->usage == SYMBOL_VARIABLE && !s->owner && s->type_symbol) {
                wasm_emit_line(mod->out, 2, "local.get $frame");
                wasm_emit_line(mod->out, 2, "i32.const %" PRIu64, layout_offset(&mod->layout, s));
                wasm_emit_line(mod->out, 2, "i32.add");
                wasm_emit_line(mod->out, 2, "call $init_%zu", layout_id(&mod->layout, s->type_symbol));
            }
        }
        return;
    }

    size_t source = layout_id(&mod->layout, layout_source(body->symbol));
    wasm_emit_line(mod->out, 2, "global.get $active_%zu", source);
    emit_fail_if(mod->out, 2, WASM_ERR_RECURSIVE_CALL, body->symbol->declaration_line);
    wasm_emit_line(mod->out, 2, "i32.const 1");
    wasm_emit_line(mod->out, 2, "global.set $active_%zu", source);
}

static void write_body_epilogue(wasm_module_t *mod, const body_t *body) {
    if (body->symbol) {
        wasm_emit_line(mod->out, 2, "i32.const 0");
        wasm_emit_line(mod->out, 2, "global.set $active_%zu", layout_id(&mod->layout, layout_source(body->symbol)));
        wasm_emit_line(mod->out, 2, "local.get $result");
    }
    wasm_emit_line(mod->out, 1, ")");
}

/* --- Recorrido de control y despacho RPN --- */

static void emit_range(body_emitter_t *e, size_t begin, size_t end) {
    for (size_t pc = begin; pc < end; pc++) {
        size_t target;
        if (has_control_jump(e->body, pc, end, &target)) {
            pc = emit_control(e, pc, target) - 1;
        } else {
            emit_step(e, pc);
        }
    }
}

static size_t emit_control(body_emitter_t *e, size_t begin, size_t target) {
    rpn_control_t control = rpn_describe_control(e->body->code, begin, target);
    if (control.loop) {
        wasm_emit_line(e->out, e->indent, "block $exit_%zu", target);
        e->indent++;
        wasm_emit_line(e->out, e->indent, "loop $loop_%zu", target);
        e->indent++;
        emit_range(e, begin, target);
        operand_t condition = pop_operand(e);
        emit_push_temp(e, condition.temp_id);
        wasm_emit_line(e->out, e->indent, "i32.eqz");
        wasm_emit_line(e->out, e->indent, "br_if $exit_%zu", target);
        emit_range(e, target + 2, control.body_end);
        wasm_emit_line(e->out, e->indent, "br $loop_%zu", target);
        e->indent--;
        wasm_emit_line(e->out, e->indent, "end");
        e->indent--;
        wasm_emit_line(e->out, e->indent, "end");
        return control.end;
    }

    emit_range(e, begin, target);
    operand_t condition = pop_operand(e);
    emit_push_temp(e, condition.temp_id);
    wasm_emit_line(e->out, e->indent, "if");
    e->indent++;
    emit_range(e, target + 2, control.body_end);
    if (control.has_else) {
        wasm_emit_line(e->out, e->indent - 1, "else");
        emit_range(e, control.false_start, control.end);
    }
    e->indent--;
    wasm_emit_line(e->out, e->indent, "end");
    return control.end;
}

static void emit_step(body_emitter_t *e, size_t pc) {
    const rpn_step_t *step = &e->body->code->steps[pc];
    wasm_emit_line(e->out, e->indent, ";; RPN %zu, source line %d", pc + 1, step->source_line);

    switch (step->kind) {
    case RPN_SYMBOL:
        emit_symbol_step(e, step->value.symbol, pc);
        return;
    case RPN_DESTINATION:
    case RPN_ARGUMENT_DESTINATION:
        emit_destination_step(e, step);
        return;
    case RPN_ATTRIBUTE:
    case RPN_ATTRIBUTE_DESTINATION:
        emit_attr_step(e, step, pc);
        return;
    case RPN_INDEX_CHECK:
        emit_index_check_step(e, step, pc);
        return;
    case RPN_CALL_FRAME:
        emit_call_frame(e, step);
        return;
    case RPN_CALL:
        emit_call(e, step);
        return;
    case RPN_OPERATOR:
        emit_operator(e, step);
        return;
    case RPN_OBJECT_VALUE:
        if (e->depth) {
            emit_materialize(e, e->stack[e->depth - 1].temp_id, step->value.symbol, 0);
        }
        return;
    case RPN_TARGET:
        report_semantic_error(step->source_line, "Unstructured jump target in WebAssembly generation");
        return;
    }
}

/* --- Traducción de cada tipo de paso RPN --- */

/* Pila C: [] -> [valor] */
static void emit_symbol_step(body_emitter_t *e, const map_node_t *s, size_t pc) {
    int line = e->body->code->steps[pc].source_line;
    type_t type = type_of_symbol(s);

    if (s->usage == SYMBOL_LEXEME) {
        if (type.kind == TYPE_CHAIN) {
            wasm_emit_line(e->out, e->indent, "i32.const %" PRIu64, layout_offset(&e->module->layout, s));
        } else if (type.kind == TYPE_INTEGER) {
            wasm_emit_line(e->out, e->indent, "i32.const %ld", strtol(s->key, NULL, 10));
        } else {
            emit_float_const(e, s->key);
        }
    } else {
        emit_symbol_address(e, s, line);
        if (type.kind != TYPE_OBJECT) {
            wasm_emit_line(e->out, e->indent, "%s.load", wasm_type(type));
        }
    }

    size_t val_temp = emit_pop_to_temp(e, type, line);
    if (type.kind == TYPE_OBJECT) {
        val_temp = emit_snapshot(e, val_temp, pc);
    }
    push_operand(e, (operand_t){type, val_temp, 0});
}

/* Pila C: [] -> [direccion_destino] */
static void emit_destination_step(body_emitter_t *e, const rpn_step_t *step) {
    if (step->kind == RPN_ARGUMENT_DESTINATION) {
        emit_push_temp(e, e->calls[e->call_depth - 1].frame_temp_id);
        wasm_emit_line(e->out, e->indent, "i32.const %" PRIu64, layout_offset(&e->module->layout, step->value.symbol));
        wasm_emit_line(e->out, e->indent, "i32.add");
    } else {
        emit_symbol_address(e, step->value.symbol, step->source_line);
    }
    size_t addr_temp = emit_pop_to_temp(e, integer_type(), step->source_line);
    push_operand(e, (operand_t){type_of_symbol(step->value.symbol), addr_temp, 0});
}

/* Pila C: [] -> [direccion_o_valor_atributo] */
static void emit_attr_step(body_emitter_t *e, const rpn_step_t *step, size_t pc) {
    size_t object_temp = 0;
    size_t index_temp = 0;
    size_t addr_temp = emit_attr_address(e, step, pc, &object_temp, &index_temp);
    type_t type = type_of_attribute(step);

    if (step->kind == RPN_ATTRIBUTE_DESTINATION) {
        push_operand(e, (operand_t){type, addr_temp, 0});
        return;
    }

    if (object_type(type)) {
        size_t copy_temp = emit_snapshot(e, addr_temp, pc);
        push_operand(e, (operand_t){type, copy_temp, 0});
        return;
    }

    size_t tag_temp = 0;
    if (type.kind == TYPE_CANDIDATES) {
        emit_push_temp(e, object_temp);
        emit_push_temp(e, index_temp);
        wasm_emit_line(e->out, e->indent, "i32.const %d", step->source_line);
        wasm_emit_line(e->out, e->indent, "call $field_type");
        tag_temp = emit_pop_to_temp(e, integer_type(), step->source_line);
    }

    emit_push_temp(e, addr_temp);
    wasm_emit_line(e->out, e->indent, "%s.load", wasm_type(type));
    size_t val_temp = emit_pop_to_temp(e, type, step->source_line);
    push_operand(e, (operand_t){type, val_temp, tag_temp});
}

static size_t emit_attr_address(body_emitter_t *e, const rpn_step_t *step, size_t pc,
                                size_t *out_object, size_t *out_index) {
    emit_symbol_step(e, step->index, pc);
    operand_t index = pop_operand(e);

    emit_symbol_address(e, step->receiver, step->source_line);
    size_t object_temp = emit_pop_to_temp(e, integer_type(), step->source_line);

    if (step->capture) {
        object_temp = emit_snapshot(e, object_temp, pc);
        emit_materialize(e, object_temp, step->capture, 0);
    }

    emit_push_temp(e, object_temp);
    emit_push_temp(e, index.temp_id);
    wasm_emit_line(e->out, e->indent, "i32.const %d", step->source_line);
    wasm_emit_line(e->out, e->indent, "call $position");

    *out_object = object_temp;
    *out_index = index.temp_id;
    return emit_pop_to_temp(e, integer_type(), step->source_line);
}

/* Pila C: [] -> [] */
static void emit_index_check_step(body_emitter_t *e, const rpn_step_t *step, size_t pc) {
    emit_symbol_step(e, step->index, pc);
    operand_t index = pop_operand(e);

    emit_symbol_address(e, step->receiver, step->source_line);
    emit_push_temp(e, index.temp_id);
    wasm_emit_line(e->out, e->indent, "i32.const %d", step->source_line);
    wasm_emit_line(e->out, e->indent, "call $position");
    wasm_emit_line(e->out, e->indent, "drop");
}

/* Pila C: [] -> [] (apila el nuevo marco en e->calls) */
static void emit_call_frame(body_emitter_t *e, const rpn_step_t *step) {
    body_t *body = find_body(e, step->value.symbol);
    wasm_emit_line(e->out, e->indent, "i32.const %" PRIu64, body->size);
    wasm_emit_line(e->out, e->indent, "i32.const %d", step->source_line);
    wasm_emit_line(e->out, e->indent, "call $allocate");
    size_t frame_temp = emit_pop_to_temp(e, integer_type(), step->source_line);

    emit_push_temp(e, frame_temp);
    emit_frame_address(e, layout_parent(step->value.symbol));
    wasm_emit_line(e->out, e->indent, "i32.store");

    const map_node_t *source = layout_source(step->value.symbol);
    if (source->usage == SYMBOL_METHOD) {
        emit_push_temp(e, frame_temp);
        if (step->receiver) {
            emit_symbol_address(e, step->receiver, step->source_line);
        } else {
            emit_receiver_address(e);
        }
        wasm_emit_line(e->out, e->indent, "i32.store offset=%d", FRAME_RECEIVER_OFFSET);
    }

    emit_initialize_frame(e, frame_temp, step->value.symbol);
    e->calls[e->call_depth++] = (call_t){step->value.symbol, frame_temp};
}

/* Pila C: [] -> [valor_retorno] (libera el marco de e->calls y restaura $sp) */
static void emit_call(body_emitter_t *e, const rpn_step_t *step) {
    call_t call = e->calls[--e->call_depth];
    emit_push_temp(e, call.frame_temp_id);
    wasm_emit_line(e->out, e->indent, "call $fn_%zu", layout_id(&e->module->layout, step->value.symbol));

    type_t type = type_of_symbol(step->value.symbol);
    size_t result_temp = emit_pop_to_temp(e, type, step->source_line);

    emit_push_temp(e, call.frame_temp_id);
    wasm_emit_line(e->out, e->indent, "i64.extend_i32_u");
    wasm_emit_line(e->out, e->indent, "global.set $sp");

    push_operand(e, (operand_t){type, result_temp, 0});
}

static void emit_operator(body_emitter_t *e, const rpn_step_t *step) {
    rpn_operator_t op = step->value.operator;
    if (op <= RPN_NOT_EQUAL && op != RPN_ASSIGN) {
        emit_binary(e, op, step->source_line);
        return;
    }
    if (op == RPN_ASSIGN) {
        emit_assign(e);
        return;
    }
    if (op == RPN_DUPLICATE) {
        /* Pila C: [a] -> [a, a] */
        operand_t value = pop_operand(e);
        push_operand(e, value);
        push_operand(e, value);
        return;
    }
    if (op == RPN_DROP) {
        /* Pila C: [a] -> [] */
        pop_operand(e);
        return;
    }
    if (op == RPN_TO_INTEGER) {
        emit_convert_integer(e, step->source_line);
        return;
    }
    if (op == RPN_OUTPUT) {
        emit_output(e);
        return;
    }
    if (op == RPN_RETURN) {
        /* Pila C: [valor] -> [] */
        operand_t value = pop_operand(e);
        emit_push_temp(e, value.temp_id);
        wasm_emit_line(e->out, e->indent, "local.set $result");
        wasm_emit_line(e->out, e->indent, "br $return");
        return;
    }
    report_semantic_error(step->source_line, "Unexpected branch outside structured region");
}

/* Pila C: [izq, der] -> [resultado] */
static void emit_binary(body_emitter_t *e, rpn_operator_t op, int line) {
    operand_t right = pop_operand(e);
    operand_t left = pop_operand(e);
    bool floating = left.type.kind == TYPE_SINGLEF;
    static const char *integer_ops[] = {"add", "sub", "mul", "div_s", "", "lt_s", "gt_s", "le_s", "ge_s", "eq", "ne"};
    static const char *float_ops[] = {"add", "sub", "mul", "div", "", "lt", "gt", "le", "ge", "eq", "ne"};
    type_t result_type = op >= RPN_LESS ? integer_type() : left.type;

    if (floating && op == RPN_ADD) {
        emit_float_add_checked(e, left, right, line);
    } else {
        emit_push_temp(e, left.temp_id);
        emit_push_temp(e, right.temp_id);
        wasm_emit_line(e->out, e->indent, "%s.%s", floating ? "f32" : "i32", floating ? float_ops[op] : integer_ops[op]);
    }

    size_t result_temp = emit_pop_to_temp(e, result_type, line);
    if (!floating && op == RPN_MULTIPLY) {
        emit_int16_overflow_check(e, result_temp, line);
    }
    if (floating && op == RPN_ADD) {
        emit_push_temp(e, result_temp);
        wasm_emit_line(e->out, e->indent, "f64.promote_f32");
        wasm_emit_line(e->out, e->indent, "f64.abs");
        wasm_emit_line(e->out, e->indent, "f64.const " FLOAT32_MAX_ABS_STR);
        wasm_emit_line(e->out, e->indent, "f64.ge");
        emit_fail_if(e->out, e->indent, WASM_ERR_FLOAT_OVERFLOW, line);
    }
    if (!floating && op < RPN_ASSIGN) {
        emit_int16_sign_extend(e, result_temp);
    }
    push_operand(e, (operand_t){result_type, result_temp, 0});
}

/* Pila C: [valor, destino] -> [] */
static void emit_assign(body_emitter_t *e) {
    operand_t destination = pop_operand(e);
    operand_t value = pop_operand(e);
    emit_push_temp(e, destination.temp_id);
    emit_push_temp(e, value.temp_id);
    if (object_type(value.type)) {
        wasm_emit_line(e->out, e->indent, "call $copy_object");
    } else {
        wasm_emit_line(e->out, e->indent, "%s.store", wasm_type(value.type));
    }
}

/* Pila C: [float] -> [integer] */
static void emit_convert_integer(body_emitter_t *e, int line) {
    operand_t value = pop_operand(e);

    emit_push_temp(e, value.temp_id);
    emit_push_temp(e, value.temp_id);
    wasm_emit_line(e->out, e->indent, "f32.ne");

    emit_push_temp(e, value.temp_id);
    wasm_emit_line(e->out, e->indent, "f32.const %d", INT16_MIN_WASM);
    wasm_emit_line(e->out, e->indent, "f32.lt");
    wasm_emit_line(e->out, e->indent, "i32.or");

    emit_push_temp(e, value.temp_id);
    wasm_emit_line(e->out, e->indent, "f32.const %d", INT16_MAX_WASM);
    wasm_emit_line(e->out, e->indent, "f32.gt");
    wasm_emit_line(e->out, e->indent, "i32.or");
    emit_fail_if(e->out, e->indent, WASM_ERR_FLOAT_TO_INT_RANGE, line);

    emit_push_temp(e, value.temp_id);
    emit_push_temp(e, value.temp_id);
    wasm_emit_line(e->out, e->indent, "f32.trunc");
    wasm_emit_line(e->out, e->indent, "f32.ne");
    emit_fail_if(e->out, e->indent, WASM_ERR_FRACTIONAL_TRUNC, line);

    emit_push_temp(e, value.temp_id);
    wasm_emit_line(e->out, e->indent, "i32.trunc_f32_s");
    push_operand(e, (operand_t){integer_type(), emit_pop_to_temp(e, integer_type(), line), 0});
}

/* Pila C: [valor] -> [] */
static void emit_output(body_emitter_t *e) {
    operand_t value = pop_operand(e);
    if (value.type.kind == TYPE_CANDIDATES) {
        emit_push_temp(e, value.tag_temp_id);
        wasm_emit_line(e->out, e->indent, "i32.const %d", TYPE_INTEGER);
        wasm_emit_line(e->out, e->indent, "i32.eq");
        wasm_emit_line(e->out, e->indent, "if");
        emit_push_temp(e, value.temp_id);
        wasm_emit_line(e->out, e->indent + 1, "call $print_integer");
        wasm_emit_line(e->out, e->indent, "else");
        emit_push_temp(e, value.temp_id);
        wasm_emit_line(e->out, e->indent + 1, "f32.reinterpret_i32");
        wasm_emit_line(e->out, e->indent + 1, "call $print_float");
        wasm_emit_line(e->out, e->indent, "end");
        return;
    }

    emit_push_temp(e, value.temp_id);
    const char *fn = value.type.kind == TYPE_SINGLEF ? "float"
                   : value.type.kind == TYPE_CHAIN   ? "chain"
                                                     : "integer";
    wasm_emit_line(e->out, e->indent, "call $print_%s", fn);
}

static void emit_materialize(body_emitter_t *e, size_t object_temp, const map_node_t *capture, layout_size_t offset) {
    typedef struct {
        const map_node_t *capture;
        layout_size_t offset;
        size_t next;
    } materialize_frame_t;

    size_t count = 1, capacity = 16;
    materialize_frame_t *stack = memory_zero(capacity, sizeof(*stack));
    stack[0] = (materialize_frame_t){capture, offset, 0};

    while (count) {
        materialize_frame_t *frame = &stack[count - 1];
        const symbol_capture_info_t *info = frame->capture->capture_info;
        const map_node_t *kind = frame->capture->type_symbol;
        if (!info || frame->next == kind->class_info->effective_attributes.count) {
            count--;
            continue;
        }

        size_t position = frame->next++;
        const map_node_t *field = kind->class_info->effective_attributes.items[position];
        layout_size_t field_offset = layout_add(
            frame->offset, layout_field_offset(&e->module->layout, kind, position), field->declaration_line);

        for (size_t j = 0; j < info->comptime_bindings.count; j++) {
            if (info->comptime_bindings.items[j] != field) {
                continue;
            }
            const map_node_t *constant = info->comptime_values.items[j];
            emit_push_temp(e, object_temp);
            wasm_emit_line(e->out, e->indent, "i32.const %" PRIu64, field_offset);
            wasm_emit_line(e->out, e->indent, "i32.add");
            if (type_of_symbol(field).kind == TYPE_INTEGER) {
                wasm_emit_line(e->out, e->indent, "i32.const %ld", strtol(constant->key, NULL, 10));
            } else {
                emit_float_const(e, constant->key);
            }
            wasm_emit_line(e->out, e->indent, "%s.store", wasm_type(type_of_symbol(field)));
        }

        for (size_t j = 0; j < info->object_fields.count; j++) {
            if (info->object_fields.items[j] != field) {
                continue;
            }
            if (count == capacity) {
                capacity = memory_multiply(capacity, 2);
                stack = memory_resize(stack, memory_multiply(capacity, sizeof(*stack)));
            }
            stack[count++] = (materialize_frame_t){info->object_values.items[j], field_offset, 0};
        }
    }
    free(stack);
}

/* --- Helpers de direcciones, chequeos y pila del emisor --- */

static void emit_frame_address(body_emitter_t *e, const map_node_t *owner) {
    const map_node_t *current = layout_source(e->body->symbol);
    owner = layout_source(owner);
    wasm_emit_line(e->out, e->indent, "local.get $frame");
    while (current && current != owner) {
        wasm_emit_line(e->out, e->indent, "i32.load");
        current = layout_parent(current);
    }
    if (current != owner) {
        report_semantic_error(owner ? owner->declaration_line : 1, "Cannot locate lexical frame");
    }
}

static void emit_receiver_address(body_emitter_t *e) {
    const map_node_t *owner = layout_source(e->body->symbol);
    while (owner && owner->usage != SYMBOL_METHOD) {
        owner = layout_parent(owner);
    }
    if (!owner) {
        report_semantic_error(1, "Missing method receiver");
        wasm_emit_line(e->out, e->indent, "i32.const 0");
        return;
    }
    emit_frame_address(e, owner);
    wasm_emit_line(e->out, e->indent, "i32.load offset=%d", FRAME_RECEIVER_OFFSET);
}

static void emit_symbol_address(body_emitter_t *e, const map_node_t *symbol, int line) {
    if (symbol->usage == SYMBOL_ATTRIBUTE) {
        emit_receiver_address(e);
        wasm_emit_line(e->out, e->indent, "i32.const %zu", layout_id(&e->module->layout, symbol));
        wasm_emit_line(e->out, e->indent, "i32.const %d", line);
        wasm_emit_line(e->out, e->indent, "call $field");
        return;
    }
    emit_frame_address(e, symbol->owner);
    wasm_emit_line(e->out, e->indent, "i32.const %" PRIu64, layout_offset(&e->module->layout, symbol));
    wasm_emit_line(e->out, e->indent, "i32.add");
}

static void emit_initialize_frame(body_emitter_t *e, size_t frame_temp, const map_node_t *function) {
    const map_node_t *source = layout_source(function);
    for (size_t i = 0; i < e->module->layout.count; i++) {
        const map_node_t *s = e->module->layout.items[i].symbol;
        if ((s->usage == SYMBOL_VARIABLE || s->usage == SYMBOL_PARAMETER) && s->owner == source && s->type_symbol) {
            emit_push_temp(e, frame_temp);
            wasm_emit_line(e->out, e->indent, "i32.const %" PRIu64, layout_offset(&e->module->layout, s));
            wasm_emit_line(e->out, e->indent, "i32.add");
            wasm_emit_line(e->out, e->indent, "call $init_%zu", layout_id(&e->module->layout, s->type_symbol));
        }
    }
}

static size_t emit_snapshot(body_emitter_t *e, size_t source_temp, size_t pc) {
    wasm_emit_line(e->out, e->indent, "local.get $frame");
    wasm_emit_line(e->out, e->indent, "i32.const %" PRIu64, e->body->scratch[pc]);
    wasm_emit_line(e->out, e->indent, "i32.add");
    size_t result_temp = emit_pop_to_temp(e, integer_type(), e->body->code->steps[pc].source_line);

    emit_push_temp(e, result_temp);
    emit_push_temp(e, source_temp);
    emit_push_temp(e, source_temp);
    wasm_emit_line(e->out, e->indent, "call $object_size");
    wasm_emit_line(e->out, e->indent, "memory.copy");
    return result_temp;
}

static void emit_float_const(body_emitter_t *e, const char *key) {
    char *text = strdup(key);
    for (char *p = text; *p; p++) {
        if (*p == 's') {
            *p = 'e';
        }
    }
    wasm_emit_line(e->out, e->indent, "f32.const %s", text);
    free(text);
}

static void emit_fail_if(FILE *out, int indent, int code, int line) {
    wasm_emit_line(out, indent, "if");
    wasm_emit_line(out, indent + 1, "i32.const %d", code);
    wasm_emit_line(out, indent + 1, "i32.const %d", line);
    wasm_emit_line(out, indent + 1, "call $fail");
    wasm_emit_line(out, indent + 1, "unreachable");
    wasm_emit_line(out, indent, "end");
}

static void emit_float_add_checked(body_emitter_t *e, operand_t left, operand_t right, int line) {
    emit_push_temp(e, left.temp_id);
    wasm_emit_line(e->out, e->indent, "f64.promote_f32");
    emit_push_temp(e, right.temp_id);
    wasm_emit_line(e->out, e->indent, "f64.promote_f32");
    wasm_emit_line(e->out, e->indent, "f64.add");
    wasm_emit_line(e->out, e->indent, "local.tee $wide");
    wasm_emit_line(e->out, e->indent, "f64.abs");
    wasm_emit_line(e->out, e->indent, "f64.const " FLOAT32_MAX_ABS_STR);
    wasm_emit_line(e->out, e->indent, "f64.ge");
    emit_fail_if(e->out, e->indent, WASM_ERR_FLOAT_OVERFLOW, line);
    wasm_emit_line(e->out, e->indent, "local.get $wide");
    wasm_emit_line(e->out, e->indent, "f32.demote_f64");
}

static void emit_int16_overflow_check(body_emitter_t *e, size_t result_temp, int line) {
    emit_push_temp(e, result_temp);
    wasm_emit_line(e->out, e->indent, "i32.const %d", INT16_MIN_WASM);
    wasm_emit_line(e->out, e->indent, "i32.lt_s");
    emit_push_temp(e, result_temp);
    wasm_emit_line(e->out, e->indent, "i32.const %d", INT16_MAX_WASM);
    wasm_emit_line(e->out, e->indent, "i32.gt_s");
    wasm_emit_line(e->out, e->indent, "i32.or");
    emit_fail_if(e->out, e->indent, WASM_ERR_INT_OVERFLOW, line);
}

static void emit_int16_sign_extend(body_emitter_t *e, size_t result_temp) {
    emit_push_temp(e, result_temp);
    wasm_emit_line(e->out, e->indent, "i32.const %d", INT16_SIGN_EXTEND_SHIFT);
    wasm_emit_line(e->out, e->indent, "i32.shl");
    wasm_emit_line(e->out, e->indent, "i32.const %d", INT16_SIGN_EXTEND_SHIFT);
    wasm_emit_line(e->out, e->indent, "i32.shr_s");
    wasm_emit_line(e->out, e->indent, "local.set $tmp_%zu_%zu", body_id(e), result_temp);
}

static size_t alloc_temp(body_emitter_t *e, type_t type, int line) {
    size_t id = ++e->temps;
    e->temp_types[id] = type.kind;
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

static size_t emit_pop_to_temp(body_emitter_t *e, type_t type, int line) {
    size_t id = alloc_temp(e, type, line);
    wasm_emit_line(e->out, e->indent, "local.set $tmp_%zu_%zu", body_id(e), id);
    return id;
}

static void emit_push_temp(body_emitter_t *e, size_t temp_id) {
    wasm_emit_line(e->out, e->indent, "local.get $tmp_%zu_%zu", body_id(e), temp_id);
}

static operand_t pop_operand(body_emitter_t *e) {
    if (!e->depth) {
        report_semantic_error(1, "Invalid WebAssembly translation stack");
        return (operand_t){0};
    }
    return e->stack[--e->depth];
}

static void push_operand(body_emitter_t *e, operand_t value) {
    e->stack[e->depth++] = value;
}

static size_t body_id(const body_emitter_t *e) {
    return layout_id(&e->module->layout, e->body->symbol);
}

static const char *wasm_type(type_t type) {
    return type.kind == TYPE_SINGLEF ? "f32" : "i32";
}

static type_t integer_type(void) {
    return (type_t){TYPE_INTEGER, NULL};
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

static body_t *find_body(body_emitter_t *e, const map_node_t *symbol) {
    for (size_t i = 1; i < e->module->count; i++) {
        if (e->module->bodies[i].symbol == symbol) {
            return &e->module->bodies[i];
        }
    }
    report_semantic_error(symbol->declaration_line, "Missing executable body '%s'", symbol->key);
    return &e->module->bodies[0];
}

static bool has_control_jump(const body_t *body, size_t pc, size_t end, size_t *target) {
    if (!body->controls[pc]) {
        return false;
    }
    /* controls[] guarda índices en base 1 para distinguir "0 = sin control" del índice 0. */
    *target = body->controls[pc] - 1;
    return *target < end;
}
