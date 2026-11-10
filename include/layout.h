#ifndef LAYOUT_H
#define LAYOUT_H
#include "types.h"
#include <inttypes.h>
#define WASM32_MEMORY_BYTES (UINT64_C(1) << 32)
#define WASM_PAGE_BYTES UINT64_C(65536)
typedef uint64_t layout_size_t;
typedef enum {
    LAYOUT_UNVISITED,
    LAYOUT_VISITING,
    LAYOUT_COMPLETE,
    LAYOUT_FAILED
} layout_state_t;
typedef struct {
    const map_node_t *symbol;
    size_t id;
    layout_size_t offset, size;
    layout_state_t state;
} layout_entry_t;
typedef struct { layout_entry_t *items; size_t count; layout_size_t chains_end; } layout_t;
layout_size_t layout_add(layout_size_t left, layout_size_t right, int line);
bool layout_init(layout_t *layout);
void layout_destroy(layout_t *layout);
const map_node_t *layout_source(const map_node_t *symbol);
const map_node_t *layout_parent(const map_node_t *symbol);
size_t layout_id(const layout_t *layout, const map_node_t *symbol);
layout_size_t layout_offset(const layout_t *layout, const map_node_t *symbol);
layout_size_t layout_class_size(const layout_t *layout, const map_node_t *kind);
layout_size_t layout_field_offset(const layout_t *layout, const map_node_t *kind, size_t position);
layout_size_t layout_frame_size(const layout_t *layout, const map_node_t *function);
#endif
