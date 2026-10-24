#ifndef COMPTIME_OBJECTS_H
#define COMPTIME_OBJECTS_H

#include "comptime_values.h"

typedef struct {
    const comptime_environment_t *source;
    comptime_environment_t *destination;
    const map_node_t *function;
    bool unknown_control;
    bool external_unknown;
    bool argument;
} comptime_copy_t;

void comptime_objects_init(void);
void comptime_objects_destroy(void);
const map_node_t *comptime_object_location(const map_node_t *parent, const map_node_t *field);
const map_node_t *comptime_object_root(const map_node_t *object);
comptime_value_t comptime_object_field(comptime_value_t object, const map_node_t *field,
                                       const comptime_environment_t *environment);
map_node_t *comptime_capture_object(comptime_value_t value, const comptime_environment_t *environment,
                                    unsigned int *serial, int line);
void comptime_copy_object_fields(const comptime_copy_t *copy, const map_node_t *object,
                                 const map_node_t *type, comptime_value_t source, int line);

#endif
