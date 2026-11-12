#include "../include/wasm_internal.h"
#include "../include/layout.h"
#include <inttypes.h>

typedef enum {
    LOOKUP_ADDR_BY_POS,
    LOOKUP_ADDR_BY_ID,
    LOOKUP_TYPE_BY_POS
} field_lookup_mode_t;

static void write_memory_helpers(wasm_module_t *mod);
static void write_object_helpers(wasm_module_t *mod);
static void write_field_helper(wasm_module_t *mod, const char *name, field_lookup_mode_t mode);
static void write_copy_helper(wasm_module_t *mod);

void wasm_emit_runtime(wasm_module_t *mod) {
    write_memory_helpers(mod);
    write_object_helpers(mod);
    write_copy_helper(mod);
}

static void write_memory_helpers(wasm_module_t *mod) {
    FILE *out = mod->out;
    /* The exclusive end may be 2^32. Keep it wide even though addresses are i32. */
    wasm_emit_block(out, 1,
        "(func $allocate (param $size i32) (param $line i32) (result i32)\n"
        "  (local $old i64) (local $end i64) (local $pages i64)\n"
        "  global.get $sp\n"
        "  local.tee $old\n"
        "  local.get $size\n"
        "  i64.extend_i32_u\n"
        "  i64.add\n"
        "  local.tee $end\n"
        "  i64.const %" PRIu64 "\n"
        "  i64.gt_u\n"
        "  if\n"
        "    i32.const %d\n"
        "    local.get $line\n"
        "    call $fail\n"
        "    unreachable\n"
        "  end",
        WASM32_MEMORY_BYTES, WASM_ERR_OUT_OF_MEMORY);

    wasm_emit_block(out, 2,
        "local.get $end\n"
        "i64.const %" PRIu64 "\n"
        "i64.add\n"
        "i64.const 16\n"
        "i64.shr_u\n"
        "local.set $pages\n"
        "local.get $pages\n"
        "memory.size\n"
        "i64.extend_i32_u\n"
        "i64.gt_u\n"
        "if\n"
        "  local.get $pages\n"
        "  memory.size\n"
        "  i64.extend_i32_u\n"
        "  i64.sub\n"
        "  i32.wrap_i64\n"
        "  memory.grow\n"
        "  i32.const -1\n"
        "  i32.eq\n"
        "  if\n"
        "    i32.const %d\n"
        "    local.get $line\n"
        "    call $fail\n"
        "    unreachable\n"
        "  end\n"
        "end",
        WASM_PAGE_BYTES - 1, WASM_ERR_OUT_OF_MEMORY);

    wasm_emit_block(out, 1,
        "  local.get $old\n"
        "  i32.wrap_i64\n"
        "  i32.const 0\n"
        "  local.get $size\n"
        "  memory.fill\n"
        "  local.get $end\n"
        "  global.set $sp\n"
        "  local.get $old\n"
        "  i32.wrap_i64\n"
        ")");
}

static void write_object_helpers(wasm_module_t *mod) {
    FILE *out = mod->out;

    wasm_emit_line(out, 1, "(func $object_size (param $object i32) (result i32)");
    for (size_t i = 0; i < mod->layout.count; i++) {
        const layout_entry_t *item = &mod->layout.items[i];
        if (item->symbol->usage != SYMBOL_CLASS) {
            continue;
        }
        wasm_emit_block(out, 2,
            "local.get $object\n"
            "i32.load\n"
            "i32.const %zu\n"
            "i32.eq\n"
            "if\n"
            "  i32.const %" PRIu64 "\n"
            "  return\n"
            "end",
            item->id, item->size);
    }
    wasm_emit_block(out, 1,
        "  unreachable\n"
        ")");

    /* Keep traversal tasks in linear memory rather than the engine's call stack. */
    wasm_emit_block(out, 1,
        "(func $init_task (param $object i32) (param $kind i32) (param $line i32) (local $task i32)\n"
        "  i32.const 8\n"
        "  local.get $line\n"
        "  call $allocate\n"
        "  local.set $task\n"
        "  local.get $task\n"
        "  local.get $object\n"
        "  i32.store\n"
        "  local.get $task\n"
        "  local.get $kind\n"
        "  i32.store offset=4\n"
        ")");

    wasm_emit_block(out, 1,
        "(func $initialize (param $object i32) (param $kind i32) (param $line i32)\n"
        "  (local $base i64) (local $task i32)\n"
        "  global.get $sp\n"
        "  local.set $base\n"
        "  local.get $object\n"
        "  local.get $kind\n"
        "  local.get $line\n"
        "  call $init_task\n"
        "  block $done\n"
        "    loop $next\n"
        "      global.get $sp\n"
        "      local.get $base\n"
        "      i64.eq\n"
        "      br_if $done\n"
        "      global.get $sp\n"
        "      i64.const 8\n"
        "      i64.sub\n"
        "      global.set $sp\n"
        "      global.get $sp\n"
        "      i32.wrap_i64\n"
        "      local.tee $task\n"
        "      i32.load\n"
        "      local.set $object\n"
        "      local.get $task\n"
        "      i32.load offset=4\n"
        "      local.set $kind\n"
        "      local.get $object\n"
        "      local.get $kind\n"
        "      i32.store");
    for (size_t i = 0; i < mod->layout.count; i++) {
        const layout_entry_t *item = &mod->layout.items[i];
        const map_node_t *kind = item->symbol;
        if (kind->usage != SYMBOL_CLASS) {
            continue;
        }
        wasm_emit_block(out, 4,
            "local.get $kind\n"
            "i32.const %zu\n"
            "i32.eq\n"
            "if",
            item->id);
        for (size_t j = 0; j < kind->class_info->effective_attributes.count; j++) {
            const map_node_t *field = kind->class_info->effective_attributes.items[j];
            if (field->type_symbol) {
                wasm_emit_block(out, 5,
                    "local.get $object\n"
                    "i32.const %" PRIu64 "\n"
                    "i32.add\n"
                    "i32.const %zu\n"
                    "local.get $line\n"
                    "call $init_task",
                    layout_field_offset(&mod->layout, kind, j),
                    layout_id(&mod->layout, field->type_symbol));
            }
        }
        wasm_emit_block(out, 4,
            "  br $next\n"
            "end");
    }
    wasm_emit_block(out, 1,
        "      unreachable\n"
        "    end\n"
        "  end\n"
        ")");

    for (size_t i = 0; i < mod->layout.count; i++) {
        const layout_entry_t *item = &mod->layout.items[i];
        if (item->symbol->usage == SYMBOL_CLASS) {
            wasm_emit_block(out, 1,
                "(func $init_%zu (param $object i32)\n"
                "  local.get $object\n"
                "  i32.const %zu\n"
                "  i32.const %d\n"
                "  call $initialize\n"
                ")",
                item->id, item->id, item->symbol->declaration_line);
        }
    }

    write_field_helper(mod, "position", LOOKUP_ADDR_BY_POS);
    write_field_helper(mod, "field", LOOKUP_ADDR_BY_ID);
    write_field_helper(mod, "field_type", LOOKUP_TYPE_BY_POS);
}

static void write_field_helper(wasm_module_t *mod, const char *name, field_lookup_mode_t mode) {
    FILE *out = mod->out;
    wasm_emit_line(out, 1, "(func $%s (param $object i32) (param $index i32) (param $line i32) (result i32)", name);
    for (size_t i = 0; i < mod->layout.count; i++) {
        const map_node_t *kind = mod->layout.items[i].symbol;
        if (kind->usage != SYMBOL_CLASS) {
            continue;
        }
        wasm_emit_block(out, 2,
            "local.get $object\n"
            "i32.load\n"
            "i32.const %zu\n"
            "i32.eq\n"
            "if",
            mod->layout.items[i].id);
        for (size_t j = 0; j < kind->class_info->effective_attributes.count; j++) {
            const map_node_t *field = kind->class_info->effective_attributes.items[j];
            size_t key = (mode == LOOKUP_ADDR_BY_ID) ? layout_id(&mod->layout, field) : j;
            wasm_emit_block(out, 3,
                "local.get $index\n"
                "i32.const %zu\n"
                "i32.eq\n"
                "if",
                key);
            if (mode == LOOKUP_TYPE_BY_POS) {
                wasm_emit_line(out, 4, "i32.const %d", type_of_symbol(field).kind);
            } else {
                wasm_emit_block(out, 4,
                    "local.get $object\n"
                    "i32.const %" PRIu64 "\n"
                    "i32.add",
                    layout_field_offset(&mod->layout, kind, j));
            }
            wasm_emit_block(out, 3,
                "  return\n"
                "end");
        }
        wasm_emit_line(out, 2, "end");
    }
    wasm_emit_block(out, 1,
        "  i32.const %d\n"
        "  local.get $line\n"
        "  call $fail\n"
        "  unreachable\n"
        ")",
        WASM_ERR_INVALID_FIELD);
}

static void write_copy_helper(wasm_module_t *mod) {
    FILE *out = mod->out;
    wasm_emit_line(out, 1, "(func $copy_object (param $destination i32) (param $source i32)");
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
            /* Compatible layouts have identical field types, including nested tags.
               Preserve only the destination's own tag and copy its entire payload. */
            wasm_emit_block(out, 2,
                "local.get $destination\n"
                "i32.load\n"
                "i32.const %zu\n"
                "i32.eq\n"
                "local.get $source\n"
                "i32.load\n"
                "i32.const %zu\n"
                "i32.eq\n"
                "i32.and\n"
                "if\n"
                "  local.get $destination\n"
                "  i32.const 4\n"
                "  i32.add\n"
                "  local.get $source\n"
                "  i32.const 4\n"
                "  i32.add\n"
                "  i32.const %" PRIu64 "\n"
                "  memory.copy\n"
                "  return\n"
                "end",
                layout_id(&mod->layout, dst),
                layout_id(&mod->layout, src),
                layout_class_size(&mod->layout, dst) - 4);
        }
    }
    wasm_emit_block(out, 1,
        "  unreachable\n"
        ")");
}
