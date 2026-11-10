#include <stdlib.h>
#include <string.h>
#include "../include/layout.h"
#include "../include/diagnostics.h"
#include "../include/memory.h"

layout_size_t layout_add(layout_size_t left, layout_size_t right, int line) {
    if (left > UINT32_MAX || right > UINT32_MAX - left) {
        report_semantic_error(line, "Storage layout is not representable in wasm32");
        return 0;
    }
    return left + right;
}

const map_node_t *layout_source(const map_node_t *symbol) {
    return symbol && symbol->function_info && symbol->function_info->source_function
        ? symbol->function_info->source_function : symbol;
}

const map_node_t *layout_parent(const map_node_t *symbol) {
    symbol = layout_source(symbol);
    symbol = symbol ? symbol->owner : NULL;
    while (symbol && symbol->usage == SYMBOL_CLASS) symbol = symbol->owner;
    return symbol;
}

static layout_entry_t *entry(const layout_t *layout, const map_node_t *symbol) {
    for (size_t i = 0; i < layout->count; i++)
        if (layout->items[i].symbol == symbol) return &layout->items[i];
    return NULL;
}

size_t layout_id(const layout_t *layout, const map_node_t *symbol) {
    layout_entry_t *item = entry(layout, symbol);
    return item ? item->id : 0;
}
layout_size_t layout_offset(const layout_t *layout, const map_node_t *symbol) {
    layout_entry_t *item = entry(layout, symbol);
    return item ? item->offset : 0;
}
layout_size_t layout_class_size(const layout_t *layout, const map_node_t *kind) {
    layout_entry_t *item = entry(layout, kind);
    return item ? item->size : 4;
}
layout_size_t layout_field_offset(const layout_t *layout, const map_node_t *kind, size_t position) {
    layout_size_t offset = 4;
    for (size_t i = 0; i < position; i++) {
        const map_node_t *field = kind->class_info->effective_attributes.items[i];
        offset = layout_add(offset, field->type_symbol ? layout_class_size(layout, field->type_symbol) : 4,
                            field->declaration_line);
    }
    return offset;
}
layout_size_t layout_frame_size(const layout_t *layout, const map_node_t *function) {
    layout_size_t size = 8;
    function = layout_source(function);
    for (size_t i = 0; i < layout->count; i++) {
        const map_node_t *s = layout->items[i].symbol;
        if ((s->usage == SYMBOL_VARIABLE || s->usage == SYMBOL_PARAMETER) && s->owner == function) {
            layout_size_t end = layout_add(layout->items[i].offset, layout->items[i].size, s->declaration_line);
            if (end > size) size = end;
        }
    }
    return size;
}

typedef struct {
    layout_entry_t *item;
    size_t next;
} measure_frame_t;

static bool measure_class(layout_t *layout, const map_node_t *kind, measure_frame_t *stack) {
    layout_entry_t *root = entry(layout, kind);
    if (root->state == LAYOUT_COMPLETE) return true;
    size_t depth = 1;
    stack[0] = (measure_frame_t){root, 0};
    root->state = LAYOUT_VISITING;
    root->size = 4;
    while (depth) {
        measure_frame_t *frame = &stack[depth - 1];
        layout_entry_t *item = frame->item;
        const symbol_list_t *fields = &item->symbol->class_info->effective_attributes;
        if (frame->next == fields->count) {
            item->state = LAYOUT_COMPLETE;
            depth--;
            continue;
        }
        const map_node_t *field = fields->items[frame->next];
        layout_entry_t *child = field->type_symbol ? entry(layout, field->type_symbol) : NULL;
        if (child && child->state == LAYOUT_VISITING) {
            report_semantic_error(field->declaration_line, "Recursive object containment in class '%s'", child->symbol->key);
            break;
        }
        if (child && child->state == LAYOUT_UNVISITED) {
            child->state = LAYOUT_VISITING;
            child->size = 4;
            stack[depth++] = (measure_frame_t){child, 0};
            continue;
        }
        if (child && child->state == LAYOUT_FAILED) break;
        layout_size_t size = layout_add(item->size, child ? child->size : 4, item->symbol->declaration_line);
        if (!size) break;
        item->size = size;
        frame->next++;
    }
    if (!depth) return true;
    while (depth) stack[--depth].item->state = LAYOUT_FAILED;
    return false;
}

static int compare_entries(const void *a, const void *b) {
    return strcmp(((const layout_entry_t *)a)->symbol->key, ((const layout_entry_t *)b)->symbol->key);
}

bool layout_init(layout_t *layout) {
    *layout = (layout_t){0};
    for (int bucket = 0; bucket < symbol_table->capacity; bucket++)
        for (map_node_t *s = symbol_table->buckets[bucket]; s; s = s->next)
            layout->count = memory_add(layout->count, 1);
    if (layout->count > UINT32_MAX) {
        report_semantic_error(1, "Symbol identifiers are not representable in wasm32");
        return false;
    }
    layout->items = memory_zero(layout->count, sizeof(*layout->items));
    size_t index = 0;
    for (int bucket = 0; bucket < symbol_table->capacity; bucket++)
        for (map_node_t *s = symbol_table->buckets[bucket]; s; s = s->next)
            layout->items[index++].symbol = s;
    qsort(layout->items, layout->count, sizeof(*layout->items), compare_entries);
    for (size_t i = 0; i < layout->count; i++) layout->items[i].id = i + 1;
    measure_frame_t *stack = memory_zero(layout->count, sizeof(*stack));
    bool valid = true;
    for (size_t i = 0; i < layout->count && valid; i++)
        if (layout->items[i].symbol->usage == SYMBOL_CLASS)
            valid = measure_class(layout, layout->items[i].symbol, stack);
    free(stack);
    if (!valid) return false;
    layout_size_t chain_offset = 16;
    for (size_t i = 0; i < layout->count; i++) {
        layout_entry_t *item = &layout->items[i];
        const map_node_t *s = item->symbol;
        if (s->usage == SYMBOL_LEXEME && !strcmp(s->value, "CHAIN")) {
            item->offset = chain_offset;
            chain_offset = layout_add(chain_offset, strlen(s->key), s->declaration_line);
            chain_offset = layout_add(chain_offset, 1, s->declaration_line);
        }
        if (s->usage == SYMBOL_VARIABLE || s->usage == SYMBOL_PARAMETER) {
            item->size = s->type_symbol ? layout_class_size(layout, s->type_symbol) : 4;
            item->offset = 8;
            for (size_t j = 0; j < i; j++) {
                const layout_entry_t *previous = &layout->items[j];
                if ((previous->symbol->usage == SYMBOL_VARIABLE || previous->symbol->usage == SYMBOL_PARAMETER) &&
                    previous->symbol->owner == s->owner) {
                    layout_size_t end = layout_add(previous->offset, previous->size, s->declaration_line);
                    if (end > item->offset) item->offset = end;
                }
            }
            layout_add(item->offset, item->size, s->declaration_line);
        }
        if (global_errors) return false;
    }
    layout->chains_end = layout_add(chain_offset, 15, 1) & ~UINT64_C(15);
    return global_errors == 0;
}
void layout_destroy(layout_t *layout) { free(layout->items); *layout = (layout_t){0}; }
