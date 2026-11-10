#ifndef LAYOUT_H
#define LAYOUT_H
#include "types.h"
typedef enum {
    LAYOUT_UNVISITED,
    LAYOUT_VISITING,
    LAYOUT_COMPLETE
} layout_state_t;
typedef struct {
    const map_node_t *symbol;
    size_t id, offset, size;
    layout_state_t state;
} layout_entry_t;
typedef struct { layout_entry_t *items; size_t count; size_t chains_end; } layout_t;
bool layout_init(layout_t *layout);
void layout_destroy(layout_t *layout);
const map_node_t *layout_source(const map_node_t *symbol);
const map_node_t *layout_parent(const map_node_t *symbol);
size_t layout_id(const layout_t *layout, const map_node_t *symbol);
size_t layout_offset(const layout_t *layout, const map_node_t *symbol);
size_t layout_class_size(const layout_t *layout, const map_node_t *kind);
size_t layout_field_offset(const layout_t *layout, const map_node_t *kind, size_t position);
size_t layout_frame_size(const layout_t *layout, const map_node_t *function);
#endif
