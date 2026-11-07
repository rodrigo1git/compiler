#ifndef SPECIALIZE_INTERNAL_H
#define SPECIALIZE_INTERNAL_H

/* Private to the five specialize modules. Other compiler phases use specialize.h. */
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

/* The coordinator owns the registry and concrete bodies. Source bodies/symbols
   are borrowed; instances own their bodies and before/after environments. */
typedef struct {
    function_template_t *templates;
    function_template_t *last_template;
    function_instance_t *instances;
    function_instance_t *last_instance;
    rpn_t main_body;
    unsigned int serial;
    unsigned int depth;
    size_t evaluation_budget;
    const evaluation_frame_t *evaluating;
    unsigned int evaluation_depth;
} specialize_state_t;

enum { SP_MAX_DEPTH = 128, SP_EVALUATION_BUDGET = 100000 };

function_template_t *sp_find_template(const specialize_state_t *state, const map_node_t *symbol);

/* Effect summaries are computed before transforming bodies. Applying effects
   mutates only the supplied caller environment, including private probe copies. */
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

/* Transform owns its traversal stack; the caller owns input/output/environment. */
void sp_transform(specialize_state_t *state, const rpn_t *input, rpn_t *output,
                  comptime_environment_t *environment, map_node_t *function, const map_node_t *receiver,
                  bool external_unknown, comptime_value_t *returned);

/* Probe never emits IR, instances or diagnostics, and copies its input environment. */
bool sp_requires_iteration(const specialize_state_t *state, const rpn_t *input, size_t begin, size_t end,
                           const comptime_environment_t *environment, const map_node_t *receiver);

comptime_value_t sp_evaluate_call(const specialize_state_t *state, function_template_t *source,
    comptime_environment_t *environment, const comptime_environment_t *arguments,
    const map_node_t *receiver, bool unknown_entry, int line);
#endif
