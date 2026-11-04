#include <stdlib.h>
#include <string.h>
#include "../include/layout.h"
#include "../include/diagnostics.h"

const map_node_t *layout_source(const map_node_t *symbol) {
    return symbol && symbol->source_function ? symbol->source_function : symbol;
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
size_t layout_offset(const layout_t *layout, const map_node_t *symbol) {
    layout_entry_t *item = entry(layout, symbol);
    return item ? item->offset : 0;
}
size_t layout_class_size(const layout_t *layout, const map_node_t *kind) {
    layout_entry_t *item = entry(layout, kind);
    return item ? item->size : 4;
}
size_t layout_field_offset(const layout_t *layout, const map_node_t *kind, size_t position) {
    size_t offset = 4;
    for (size_t i = 0; i < position; i++) {
        const map_node_t *field = kind->effective_attributes.items[i];
        offset += field->type_symbol ? layout_class_size(layout, field->type_symbol) : 4;
    }
    return offset;
}
size_t layout_frame_size(const layout_t *layout, const map_node_t *function) {
    size_t size = 8;
    function = layout_source(function);
    for (size_t i = 0; i < layout->count; i++) {
        const map_node_t *s = layout->items[i].symbol;
        if ((s->usage == SYMBOL_VARIABLE || s->usage == SYMBOL_PARAMETER) && s->owner == function)
            if (layout->items[i].offset + layout->items[i].size > size)
                size = layout->items[i].offset + layout->items[i].size;
    }
    return size;
}

static size_t measure_class(layout_t *layout, const map_node_t *kind) {
    layout_entry_t *item = entry(layout, kind);
    if (item->state == LAYOUT_COMPLETE) return item->size;
    if (item->state == LAYOUT_VISITING) {
        report_semantic_error(kind->declaration_line, "Recursive object containment in class '%s'", kind->key);
        return 4;
    }
    item->state = LAYOUT_VISITING;
    size_t size = 4;
    for (size_t i = 0; i < kind->effective_attributes.count; i++) {
        const map_node_t *field = kind->effective_attributes.items[i];
        size += field->type_symbol ? measure_class(layout, field->type_symbol) : 4;
        if (size > 1048576) {
            report_semantic_error(kind->declaration_line, "Object layout exceeds implementation memory limit"); break;
        }
    }
    item->size = size;
    item->state = LAYOUT_COMPLETE;
    return size;
}

static int compare_entries(const void *a, const void *b) {
    return strcmp(((const layout_entry_t *)a)->symbol->key, ((const layout_entry_t *)b)->symbol->key);
}

bool layout_init(layout_t *layout) {
    *layout = (layout_t){0};
    for (int bucket = 0; bucket < symbol_table->capacity; bucket++)
        for (map_node_t *s = symbol_table->buckets[bucket]; s; s = s->next) layout->count++;
    layout->items = calloc(layout->count, sizeof(*layout->items));
    if (!layout->items) { fprintf(stderr, "Cannot allocate storage plan\n"); exit(1); }
    size_t index = 0;
    for (int bucket = 0; bucket < symbol_table->capacity; bucket++)
        for (map_node_t *s = symbol_table->buckets[bucket]; s; s = s->next)
            layout->items[index++].symbol = s;
    qsort(layout->items, layout->count, sizeof(*layout->items), compare_entries);
    for (size_t i = 0; i < layout->count; i++) layout->items[i].id = i + 1;
    for (size_t i = 0; i < layout->count; i++)
        if (layout->items[i].symbol->usage == SYMBOL_CLASS) measure_class(layout, layout->items[i].symbol);
    size_t string_offset = 16;
    for (size_t i = 0; i < layout->count; i++) {
        layout_entry_t *item = &layout->items[i];
        const map_node_t *s = item->symbol;
        if (s->usage == SYMBOL_LEXEME && !strcmp(s->value, "CHAIN")) {
            item->offset = string_offset;
            string_offset += strlen(s->key) + 1;
        }
        if (s->usage == SYMBOL_VARIABLE || s->usage == SYMBOL_PARAMETER) {
            item->size = s->type_symbol ? layout_class_size(layout, s->type_symbol) : 4;
            item->offset = 8;
            for (size_t j = 0; j < i; j++) {
                const layout_entry_t *previous = &layout->items[j];
                if ((previous->symbol->usage == SYMBOL_VARIABLE || previous->symbol->usage == SYMBOL_PARAMETER) &&
                    previous->symbol->owner == s->owner && previous->offset + previous->size > item->offset)
                    item->offset = previous->offset + previous->size;
            }
        }
    }
    layout->strings_end = (string_offset + 15) & ~(size_t)15;
    return global_errors == 0;
}
void layout_destroy(layout_t *layout) { free(layout->items); *layout = (layout_t){0}; }
