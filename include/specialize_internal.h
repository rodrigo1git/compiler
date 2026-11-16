#ifndef SPECIALIZE_INTERNAL_H
#define SPECIALIZE_INTERNAL_H

#include "specialize.h"

typedef struct function_template {
    map_node_t *symbol;
    const rpn_t *body;
    bool contextual;
    bool external_writes;
    bool writes_attributes;
    symbol_list_t runtime_writes;
    bool used;
    struct function_template *next;
} function_template_t;

typedef struct function_instance {
    function_template_t *source;
    map_node_t *symbol;
    comptime_environment_t before;
    comptime_environment_t after;
    rpn_t body;
    comptime_value_t returned;
    bool active;
    bool unknown_entry;
    const map_node_t *receiver;
    struct function_instance *next;
} function_instance_t;

typedef struct evaluation_frame {
    const map_node_t *function;
    const struct evaluation_frame *parent;
} evaluation_frame_t;

typedef struct {
    function_template_t *templates;
    function_template_t *last_template;
    function_instance_t *instances;
    function_instance_t *last_instance;
    rpn_t main_body;
    unsigned int serial;
    const evaluation_frame_t *evaluating;
} specialize_state_t;

function_template_t *sp_find_template(const specialize_state_t *state, const map_node_t *symbol);

void sp_analyze_effects(specialize_state_t *state);
bool sp_has_comptime_effects(const specialize_state_t *state, const rpn_t *input, size_t begin, size_t end);
void sp_forget_call_writes(comptime_environment_t *environment, const function_template_t *source);
void sp_apply_effects(const function_template_t *source, const comptime_environment_t *after,
                      const map_node_t *from_receiver, comptime_environment_t *caller,
                      const map_node_t *receiver);

function_instance_t *sp_find_active_instance(const specialize_state_t *state,
                                             const function_template_t *source);
function_instance_t *sp_instantiate(specialize_state_t *state, function_template_t *source,
                                    comptime_environment_t *caller, const comptime_environment_t *arguments,
                                    const map_node_t *receiver, bool unknown_entry, int line);
void sp_destroy_instances(specialize_state_t *state);

typedef struct transform_context context_t;
void sp_transform_range(context_t *context, size_t begin, size_t end, bool unknown_control,
                        bool inspect_first_control);
void sp_transform(specialize_state_t *state, const rpn_t *input, rpn_t *output,
                  comptime_environment_t *environment, map_node_t *function, const map_node_t *receiver,
                  bool external_unknown, comptime_value_t *returned);

typedef struct probe probe_t;
void sp_probe_range(probe_t *probe, size_t begin, size_t end, bool inspect_first);
bool sp_requires_iteration(const specialize_state_t *state, const rpn_t *input, size_t begin, size_t end,
                           const comptime_environment_t *environment, const map_node_t *receiver);

comptime_value_t sp_evaluate_call(const specialize_state_t *state, function_template_t *source,
    comptime_environment_t *environment, const comptime_environment_t *arguments,
    const map_node_t *receiver, bool unknown_entry, int line);
#endif
