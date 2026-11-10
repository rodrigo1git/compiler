#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "classes.h"
#include "comptime_objects.h"
#include "diagnostics.h"
#include "layout.h"
#include "memory.h"
#include "scope.h"

static void deep_classes(void) {
    const size_t count = 5000;
    map_node_t **classes = memory_zero(count, sizeof(*classes));
    symbol_table = map_create(8192);
    set_trace_enabled(false);
    scope_init();
    for (size_t i = 0; i < count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "c%zu", i);
        scope_t *scope = scope_begin(map_put(symbol_table, name, "ID"), SYMBOL_CLASS, (scope_type_t){0}, 1);
        classes[i] = scope_symbol(scope);
        scope_end(scope);
    }
    for (size_t i = 0; i + 1 < count; i++) class_add_base(classes[i], classes[i + 1]);
    assert(class_build_layout(classes[0], 1));
    class_member_t missing = class_lookup_member(classes[0], "missing", CLASS_ALL_MEMBERS);
    assert(!missing.symbol && !missing.cyclic && !missing.ambiguous);
    /* Replace inheritance edges with a long, finite by-value object chain. */
    for (size_t i = 0; i < count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "field%zu", i);
        map_node_t *field = map_put(symbol_table, name, "ID");
        symbol_set_usage(field, SYMBOL_ATTRIBUTE);
        field->type_symbol = i + 1 < count ? classes[i + 1] : NULL;
        field->is_comptime = i + 1 == count;
        symbol_list_append(&classes[i]->class_info->effective_attributes, field);
    }
    assert(class_check_containment());
    assert(class_has_comptime(classes[0]));
    layout_t layout;
    assert(layout_init(&layout));
    assert(layout_class_size(&layout, classes[0]) == 4 * (count + 1));
    assert(layout_class_size(&layout, classes[count - 1]) == 8);
    layout_destroy(&layout);
    /* Exercise deep captures and knowledge copies, not just the class graph. */
    const size_t start = count - 1200;
    map_node_t *source = map_put(symbol_table, "source", "ID");
    map_node_t *target = map_put(symbol_table, "target", "ID");
    source->type_symbol = target->type_symbol = classes[start];
    comptime_objects_init();
    const map_node_t *source_leaf = source, *target_leaf = target;
    for (size_t i = start; i + 1 < count; i++) {
        map_node_t *field = classes[i]->class_info->effective_attributes.items[0];
        source_leaf = comptime_object_location(source_leaf, field);
        target_leaf = comptime_object_location(target_leaf, field);
    }
    map_node_t *leaf_field = classes[count - 1]->class_info->effective_attributes.items[0];
    comptime_environment_t before = {0}, after = {0};
    comptime_set(&before, leaf_field, source_leaf, (comptime_value_t){.known = true, .number = 7});
    comptime_value_t value = {.object = source, .object_type = classes[start]};
    unsigned int serial = 0;
    map_node_t *snapshot = comptime_capture_object(value, &before, &serial, 1);
    assert(serial == count - start);
    for (size_t i = start; i + 1 < count; i++) snapshot = snapshot->capture_info->object_values.items[0];
    assert(!strcmp(snapshot->capture_info->comptime_values.items[0]->key, "7$i"));
    comptime_copy_t copy = {.source = &before, .destination = &after};
    comptime_copy_object_fields(&copy, target, classes[start], value, 1);
    comptime_value_t copied = comptime_get(&after, leaf_field, target_leaf);
    assert(copied.known && copied.number == 7);
    free(before.items);
    free(after.items);
    comptime_objects_destroy();
    scope_destroy();
    map_free(symbol_table);
    free(classes);
}

static void long_diagnostics(void) {
    char *context = memory_zero(5001, 1);
    memset(context, 'x', 5000);
    memcpy(context + 4989, "CONTEXT_END", 11);
    begin_syntax_error(11, "identifier", context);
    memset(context, 'y', 5000); /* Recovery must own an independent copy. */
    report_recovered_syntax_error_with_context(11, "Recovered message");
    finish_error_recovery();
    begin_syntax_error(12, "identifier", "flushed context");
    begin_lexical_recovery();
    finish_error_recovery();
    begin_syntax_error(13, "identifier", NULL);
    finish_error_recovery();
    assert(global_errors == 3);
    free(context);
    char *message = NULL;
    for (int i = 0; i < 100; i++) diagnostic_append(&message, "%s", "0123456789");
    assert(strlen(message) == 1000 && !strcmp(message + 990, "0123456789"));
    free(message);
}

static void chain_alignment(void) {
    const struct { size_t data_end; layout_size_t frame_base; } cases[] = {
        {16, 16}, {17, 32}, {31, 32}, {32, 32},
        {65535, 65536}, {65536, 65536}, {65537, 65552}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        symbol_table = map_create(16);
        if (cases[i].data_end > 16) {
            size_t length = cases[i].data_end - 17;
            char *text = memory_zero(length + 1, 1);
            memset(text, 'z', length);
            map_node_t *chain = map_put(symbol_table, text, "CHAIN");
            free(text);
            layout_t layout;
            assert(layout_init(&layout));
            assert(layout_offset(&layout, chain) == 16);
            assert(layout.chains_end == cases[i].frame_base);
            assert(layout.chains_end >= cases[i].data_end);
            layout_destroy(&layout);
        } else {
            layout_t layout;
            assert(layout_init(&layout));
            assert(layout.chains_end == cases[i].frame_base);
            layout_destroy(&layout);
        }
        map_free(symbol_table);
    }
}

int main(int argc, char **argv) {
    assert(argc == 2);
    if (!strcmp(argv[1], "deep")) deep_classes();
    else if (!strcmp(argv[1], "diagnostics")) long_diagnostics();
    else if (!strcmp(argv[1], "alignment")) chain_alignment();
    else if (!strcmp(argv[1], "overflow")) memory_add(SIZE_MAX, 1);
    else if (!strcmp(argv[1], "multiply")) memory_multiply(SIZE_MAX, 2);
    else if (!strcmp(argv[1], "layout")) {
        assert(layout_add(UINT32_MAX - 4, 4, 20) == UINT32_MAX);
        assert(layout_add(UINT32_MAX - 4, 5, 21) == 0);
        assert(layout_add(UINT64_MAX, 1, 22) == 0);
        /* Last possible aligned base, then the first end that rounds to 4 GiB. */
        assert(layout_add(UINT32_MAX - 15, 15, 23) == UINT32_MAX);
        assert(layout_add(UINT32_MAX - 14, 15, 24) == 0);
        assert(global_errors == 3);
    } else return 2;
    return 0;
}
