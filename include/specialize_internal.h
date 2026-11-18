#ifndef SPECIALIZE_INTERNAL_H
#define SPECIALIZE_INTERNAL_H

#include "specialize.h"

struct function_instance;

typedef struct function_template {
    map_node_t *symbol;
    const rpn_t *body;
    bool contextual;
    bool external_writes;
    bool writes_attributes;
    symbol_list_t runtime_writes;
    bool used;
    struct function_instance *active_instance;
    bool evaluating;
    bool exploring;
} function_template_t;

typedef struct function_instance {
    function_template_t *source;
    map_node_t *symbol;
    comptime_environment_t before;
    comptime_environment_t after;
    rpn_t body;
    comptime_value_t returned;
    bool unknown_entry;
    map_node_t *receiver;
} function_instance_t;

typedef struct {
    map_node_t *symbol;
    rpn_step_kind_t kind;
    map_node_t *object;
    map_node_t *receiver;
    map_node_t *index;
} sp_destination_t;

typedef struct sp_pending_call {
    function_template_t *source;
    map_node_t *receiver;
    comptime_environment_t arguments;
    size_t frame_step;
    int errors;
    struct sp_pending_call *parent;
} sp_pending_call_t;

typedef struct {
    function_template_t **items;
    size_t count;
    size_t capacity;
} sp_template_list_t;

typedef struct {
    function_instance_t **items;
    size_t count;
    size_t capacity;
} sp_instance_list_t;

typedef struct {
    sp_template_list_t templates;
    sp_instance_list_t instances;
    rpn_t main_body;
    unsigned int serial;
} specialize_state_t;

typedef struct {
    specialize_state_t *state;
    const rpn_t *input;
    rpn_t *output;
    comptime_environment_t *environment;
    map_node_t *function;
    map_node_t *receiver;
    bool external_unknown;
    comptime_value_t *returned;
} sp_transform_request_t;

typedef struct {
    bool has_value;
    comptime_value_t value;
} sp_return_state_t;

typedef struct {
    bool unknown_control;
    bool inspect_first;
} sp_range_options_t;

function_template_t *sp_find_template(const specialize_state_t *state, const map_node_t *symbol);
sp_pending_call_t *sp_pending_push(sp_pending_call_t **pending, function_template_t *source,
                                   map_node_t *receiver, size_t frame_step, int errors);
void sp_pending_pop(sp_pending_call_t **pending);
void sp_pending_free_all(sp_pending_call_t **pending);
void sp_record_return(sp_return_state_t *state, comptime_value_t value);
bool sp_is_forbidden_comptime_write(const map_node_t *symbol, const map_node_t *object,
                                    const map_node_t *function, bool argument,
                                    bool unknown_control, bool external_unknown);
bool sp_comptime_arguments_known(const function_template_t *source,
                                 const comptime_environment_t *arguments,
                                 int line, bool report);
map_node_t *sp_attribute_at(map_node_t *type, comptime_value_t index, int line);

void sp_analyze_effects(specialize_state_t *state);
bool sp_has_comptime_effects(const specialize_state_t *state, const rpn_t *input, size_t begin, size_t end);
void sp_forget_call_writes(comptime_environment_t *environment, const function_template_t *source);
void sp_apply_effects(const function_template_t *source, const comptime_environment_t *after,
                      const map_node_t *from_receiver, comptime_environment_t *caller,
                      map_node_t *receiver);

function_instance_t *sp_instantiate(specialize_state_t *state, function_template_t *source,
                                    comptime_environment_t *caller, const comptime_environment_t *arguments,
                                    map_node_t *receiver, bool unknown_entry, int line);
void sp_destroy_instances(specialize_state_t *state);

typedef struct transform_context context_t;
void sp_transform_range(context_t *context, size_t begin, size_t end, sp_range_options_t options);
void sp_transform(const sp_transform_request_t *request);

typedef struct probe probe_t;
void sp_probe_range(probe_t *probe, size_t begin, size_t end, bool inspect_first);
bool sp_requires_iteration(specialize_state_t *state, const rpn_t *input, size_t begin, size_t end,
                           const comptime_environment_t *environment, map_node_t *receiver);
comptime_value_t sp_evaluate_call(specialize_state_t *state, function_template_t *source,
                                  comptime_environment_t *environment,
                                  const comptime_environment_t *arguments,
                                  map_node_t *receiver, bool unknown_entry, int line);
bool sp_check_discarded(specialize_state_t *state, const rpn_t *body, map_node_t *function,
                        size_t begin, size_t end, const comptime_environment_t *environment,
                        map_node_t *receiver);

#endif
