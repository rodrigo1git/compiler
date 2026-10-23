"""Executable TP3 examples, applicable requirements and confirmed language rules."""
from itertools import permutations
from textwrap import dedent


CASES = []


def program(declarations, body):
    return "course_program\n" + dedent(declarations).strip() + "\nbegin\n" + dedent(body).strip() + "\nend;\n"


def case(name, reference, declarations, body, output=None, error=None, **expectations):
    item = {"name": name, "reference": reference, "source": program(declarations, body), **expectations}
    if error is not None:
        item["error"] = error
    elif output is not None:
        item["output"] = output
    CASES.append(item)


case("scope_mangling_example", "TP3 p.2", """
    integer a;
    integer function aa(integer x)
    integer a;
    integer function aaa(integer w)
    integer x;
    begin x := w; ret(a+x); end;
    begin a := x; ret(aaa(2$i)[1$i]); end;
    integer function bb(integer z)
    integer a;
    begin a := z; ret(a); end;
    integer function cc(integer y)
    integer a;
    begin a := y; ret(a); end;
""", """
    a := 100$i;
    pout(aa(3$i)[1$i]); pout(bb(4$i)[1$i]); pout(cc(5$i)[1$i]); pout(a);
""", [5, 4, 5, 100], symbols={
    "a:main": {"Declared type": "INTEGER", "Use": "VARIABLE"},
    "aa:main": {"Parameters": "[x:main:aa]", "Use": "FUNCTION"},
    "a:main:aa": {"Declared type": "INTEGER"},
    "x:main:aa:aaa": {"Declared type": "INTEGER"},
    "w:main:aa:aaa": {"Use": "PARAMETER"},
    "a:main:bb": {"Declared type": "INTEGER"},
    "a:main:cc": {"Declared type": "INTEGER"},
})

for name, declarations, body, error in [
    ("variable_undeclared", "integer a;", "a := absent;", "not declared or not visible"),
    ("variable_redeclared", "integer a; singlef a;", "a := 1$i;", "already declared"),
    ("function_undeclared", "integer a;", "a := absent(1$i)[1$i];", "not declared or not visible"),
    ("function_redeclared", "integer function f(integer p) begin ret(p); end;\n"
     "integer function f(integer p) begin ret(p); end;", "pout(1$i);", "already declared"),
    ("class_undeclared", "missing object;", "pout(1$i);", "not declared or not visible"),
    ("class_redeclared", "class c code begin integer x; end;\n"
     "class c other begin integer y; end;", "pout(1$i);", "already declared"),
    ("method_undeclared", "integer a; class c code begin integer x; end; c object;",
     "a := object.absent(1$i)[1$i];", "does not exist"),
    ("method_redeclared", "class c code begin integer m(integer p) begin ret(p); end;\n"
     "integer m(integer q) begin ret(q); end; end;", "pout(1$i);", "already declared"),
    ("function_as_destination", "integer function f(integer p) begin ret(p); end;",
     "f := 1$i;", "not a variable"),
    ("variable_as_function", "integer f, a;", "a := f(1$i)[1$i];", "not a function or method"),
    ("class_as_value", "integer a; class c code begin integer x; end;",
     "a := c;", "not a variable"),
    ("variable_as_class", "integer c; c object;", "pout(1$i);", "not a class"),
    ("member_wrong_use", "integer a; class c code begin integer m; end; c object;",
     "a := object.m(1$i)[1$i];", "not a method"),
    ("receiver_wrong_use", "integer a;", "a := a.m(1$i)[1$i];", "not an object"),
    ("later_function_hidden", "integer function early(integer p) begin ret(later(p)[1$i]); end;\n"
     "integer function later(integer p) begin ret(p); end;", "pout(early(1$i)[1$i]);",
     "not declared or not visible"),
    ("sibling_local_hidden", "integer function first(integer p) integer local; begin local := p; ret(local); end;\n"
     "integer function second(integer q) begin ret(local); end;", "pout(1$i);", "not declared or not visible"),
    ("class_later_hidden", "integer function f(integer p) later object; begin ret(p); end;\n"
     "class later code begin integer x; end;", "pout(1$i);", "not declared or not visible"),
    ("method_later_hidden", "class c code begin integer early(integer p) begin ret(later(p)[1$i]); end;\n"
     "integer later(integer p) begin ret(p); end; end;", "pout(1$i);", "not declared or not visible"),
    ("namespaces_same_scope", "integer c; class c code begin integer x; end;", "pout(1$i);", "already declared"),
]:
    case(name, "TP3 p.3", declarations, body, error=error)

case("class_and_function_shadowing", "TP3 p.3", """
    integer value;
    class box globalcode begin integer x; end;
    integer function f(integer p) begin ret(p); end;
    integer function outer(integer p)
    class box localcode begin integer x; end;
    box object;
    integer function f(integer q) begin ret(q+1$i); end;
    begin object[0$i] := p; ret(f(object[0$i])[1$i]); end;
""", "pout(f(2$i)[1$i]); pout(outer(2$i)[1$i]);", [2, 3], symbols={
    "object:main:outer": {"Declared type": "box:main:outer"},
    "f:main:outer": {"Use": "FUNCTION"},
})

FUNCTION = "integer function f(integer p1, integer p2, integer p3) begin ret(p1+p2+p3); end;"
for order in permutations((1, 2, 3)):
    suffix = "".join(map(str, order))
    positions = ",".join(f"{i}$i" for i in order)
    expected = 10 if order.index(2) < order.index(3) else 8
    case("argument_order_" + suffix, "TP3 p.7, topic 19", "integer a,b;\n" + FUNCTION,
         f"a := 1$i; b := 2$i; pout(f(a,b=4$i,a+b)[{positions}]); pout(b);", [expected, 4])
    state, actual = {"a": 1, "b": 2}, {}
    for position in order:
        if position == 1:
            state["a"] = 3
            actual[position] = 3
        elif position == 2:
            state["b"] = 4
            actual[position] = 4
        else:
            actual[position] = state["a"] + state["b"]
    expected = 100 * actual[1] + 10 * actual[2] + actual[3]
    for staged in (False, True):
        modifier = "comptime " if staged else ""
        case(("comptime_" if staged else "") + "formal_positions_" + suffix,
             "TP3 pp.5,7-8", "integer a,b;\n" +
             f"integer function f({modifier}integer p1,{modifier}integer p2,{modifier}integer p3) "
             "begin ret(100$i*p1+10$i*p2+p3); end;",
             f"a := 1$i; b := 2$i; pout(f(a=3$i,b=4$i,a+b)[{positions}]); pout(a); pout(b);",
             [expected, 3, 4])

for name, call, error in [
    ("arity_short", "f(1$i,2$i)[1$i,2$i,3$i]", "has 2 arguments; expected 3"),
    ("arity_long", "f(1$i,2$i,3$i,4$i)[1$i,2$i,3$i]", "has 4 arguments; expected 3"),
    ("order_short", "f(1$i,2$i,3$i)[1$i,2$i]", "has 2 entries; expected 3"),
    ("order_long", "f(1$i,2$i,3$i)[1$i,2$i,3$i,1$i]", "has 4 entries; expected 3"),
    ("order_duplicate", "f(1$i,2$i,3$i)[1$i,1$i,3$i]", "repeats position"),
    ("order_zero", "f(1$i,2$i,3$i)[0$i,2$i,3$i]", "outside 1..3"),
    ("order_negative", "f(1$i,2$i,3$i)[-1$i,2$i,3$i]", "outside 1..3"),
    ("order_too_large", "f(1$i,2$i,3$i)[1$i,2$i,4$i]", "outside 1..3"),
    ("order_float", "f(1$i,2$i,3$i)[1.0,2$i,3$i]", "must be INTEGER"),
    ("order_missing", "f(1$i,2$i,3$i)", "Missing order"),
]:
    case(name, "TP3 pp.5,7; confirmed permutation rule", FUNCTION, f"pout({call});", error=error)

case("parameter_copy_and_defaults", "TP3 p.5", """
    integer a;
    integer function change(integer p) begin p := 9$i; ret(p); end;
    integer function implicit(integer p) begin pout(p); end;
    singlef function implicit_float(singlef p) begin pout(p); end;
    integer function unused(integer p) begin ret(p); end;
""", "a := 3$i; pout(change(a)[1$i]); pout(a); pout(implicit(2$i)[1$i]);"
     "pout(implicit_float(1.5)[1$i]);", [9, 3, 2, 0, 1.5, 0.0],
     functions=["change:main", "implicit:main", "implicit_float:main", "unused:main"])

case("nested_and_recursive_calls", "TP3 p.5", """
    integer function sum(integer p,integer q) begin ret(p+q); end;
    integer function total(integer p)
    begin if(p>0$i) ret(p+total(p-1$i)[1$i]); end_if; ret(p); end;
""", "pout(sum(1$i,sum(2$i,3$i)[1$i,2$i])[1$i,2$i]); pout(total(5$i)[1$i]);", [6, 15])

case("returns_in_nested_control", "TP3 pp.5-6", """
    integer function f(integer p)
    integer i;
    begin
        from i := 0$i to 3$i by 1$i repeat begin
            if(p>i) begin if(i==1$i) ret(9$i); end_if; end
            else ret(4$i); end_if;
        end;
    end;
""", "pout(f(2$i)[1$i]); pout(f(0$i)[1$i]);", [9, 4])

for name, initial, final, step, expected in [
    ("from_ascending_example", 1, 10, 1, list(range(1, 10))),
    ("from_descending_example", 10, 0, 2, [10, 8, 6, 4, 2]),
    ("from_equal", 2, 2, 1, []),
    ("from_ascending_overshoot", 1, 8, 3, [1, 4, 7]),
    ("from_descending_overshoot", 8, 1, 3, [8, 5, 2]),
]:
    case(name, "TP3 p.6, topic 14", "integer i;",
         f"from i := {initial}$i to {final}$i by {step}$i repeat pout(i);;", expected)

case("from_body_changes_control", "TP3 p.6", "integer i;", """
    from i := 0$i to 5$i by 1$i repeat begin
        pout(i);
        if(i==1$i) i := 3$i; end_if;
    end;
    pout(i);
""", [0, 1, 4, 5])
case("nested_loops_and_selection", "TP3 pp.1,6", "integer i,j;", """
    from i := 0$i to 2$i by 1$i repeat begin
        from j := 2$i to 0$i by 1$i repeat begin
            if(i==0$i) pout(j); else pout(i+j); end_if;
        end;
    end;
""", [2, 1, 3, 2])

case("embedded_assignment_values", "TP3 p.7, topic 18", "integer a,b,c,d,e;", """
    c := 12$i; d := 6$i;
    a := b=c+d*e=c/3$i;
    pout(a); pout(b); pout(e);
    a := b=c=4$i;
    pout(a); pout(b); pout(c);
    a := b+b=2$i;
    pout(a); pout(b);
""", [36, 12, 12, 4, 4, 4, 6, 2])

case("comptime_update_example", "TP3 p.8, valid example 1", "comptime integer x; integer z;", """
    x := 3$i; x := x+1$i; z := x; pout(z);
""", [4], symbols={"x:main": {"Compile-time value": "4$i"}})
case("comptime_selection_example", "TP3 p.8, valid example 2", "comptime integer x; integer z;", """
    x := 3$i; z := 0$i; if(x>1$i) z := z+1$i; end_if; pout(z);
""", [1])
case("comptime_loop_example", "TP3 p.8, valid example 3 adapted to topic 14",
     "comptime integer x;", "from x := 3$i to 10$i by 1$i repeat pout(x);; pout(x);",
     list(range(3, 11)), symbols={"x:main": {"Compile-time value": "10$i"}})
case("comptime_parameter_example", "TP3 p.8, valid example 4",
     "integer function f(comptime integer x) begin pout(x); end;", "pout(f(3$i)[1$i]);", [3, 0])
case("comptime_to_runtime_parameter", "TP3 p.8, valid example 5", """
    integer function f(integer p) begin pout(p); ret(p); end;
    comptime integer x;
    integer z;
""", "x := 3$i; z := f(x)[1$i]; pout(z);", [3, 3])
case("comptime_runtime_assignment_error", "TP3 p.8, invalid example 1",
     "comptime integer x; integer z;", "x := 3$i; x := x+1$i; x := z;",
     error="not known at compile time", error_at="x := z;")
case("comptime_unknown_control_error", "TP3 p.8, invalid example 2 adapted to topic 14", """
    integer function f(integer z)
    comptime integer x;
    integer i;
    begin
        x := 3$i;
        from i := 0$i to 2$i by 1$i repeat begin
            if(x<z) x := x+1$i; end_if;
        end;
        ret(x);
    end;
""", "pout(f(10$i)[1$i]);", error="under unknown control", error_at="if(x<z) x := x+1$i;")
case("comptime_unknown_argument_error", "TP3 p.8, invalid example 3", """
    integer z;
    integer function f(comptime integer x) begin pout(x); end;
""", "pout(f(z)[1$i]);", error="not known at compile time", error_at="pout(f(z)[1$i]);")

case("comptime_branch_rejects_equal_writes", "Confirmed unknown-control rule", """
    integer function f(integer p)
    comptime integer x;
    begin
        x := 3$i;
        if(p>0$i)
            x := 4$i;
        else
            x := 4$i;
        end_if;
        ret(x);
    end;
""", "pout(f(0$i)[1$i]);", error="under unknown control", error_occurrences=2)
case("comptime_known_branch_updates", "TP3 p.8; confirmed control rule", "comptime integer x;", """
    x := 3$i;
    if(x>1$i) x := 5$i; else x := 7$i; end_if;
    pout(x);
    if(x<1$i) x := 9$i; else x := x+1$i; end_if;
    pout(x);
""", [5, 6])
case("comptime_unknown_branch_reads", "TP3 p.8", """
    comptime integer x;
    integer function f(integer p)
    begin if(p>0$i) pout(x); else pout(x+1$i); end_if; ret(x); end;
""", "x := 3$i; pout(f(1$i)[1$i]); pout(f(0$i)[1$i]);", [3, 3, 4, 3])
case("comptime_calls_propagate_guard", "TP3 p.8; confirmed control rule", """
    comptime integer x;
    integer function change(integer p)
    begin
        x := 5$i;
        ret(p);
    end;
    integer function f(integer p)
    begin if(p>0$i) ret(change(p)[1$i]); end_if; ret(p); end;
""", "x := 3$i; pout(change(0$i)[1$i]); pout(f(0$i)[1$i]);",
     error="under unknown control", error_at="x := 5$i;")
case("comptime_function_local_state", "TP3 pp.5,8", """
    integer function f(integer p)
    comptime integer x;
    begin x := 3$i; x := x+1$i; ret(x+p); end;
""", "pout(f(1$i)[1$i]); pout(f(2$i)[1$i]);", [5, 6])
case("comptime_multiple_specializations", "TP3 p.8", """
    integer function f(comptime integer x)
    begin x := x+1$i; pout(x); ret(x); end;
    comptime integer value;
""", "value := 3$i; pout(f(value)[1$i]); pout(f(7$i)[1$i]); pout(value);", [4, 4, 8, 8, 3])
case("comptime_capture_changes", "TP3 pp.3,8", """
    comptime integer x;
    integer function f(integer p) begin ret(x+p); end;
""", "x := 3$i; pout(f(1$i)[1$i]); x := 7$i; pout(f(1$i)[1$i]);", [4, 8])
case("comptime_nested_capture", "TP3 pp.2,5,8", """
    integer function outer(comptime integer p)
    comptime integer local;
    integer function inner(comptime integer q)
    begin local := local+q; ret(local); end;
    begin local := 10$i; ret(inner(p)[1$i]); end;
""", "pout(outer(1$i)[1$i]); pout(outer(2$i)[1$i]); pout(outer(1$i)[1$i]);", [11, 12, 11])
case("comptime_recursive_parameters", "TP3 pp.5,8", """
    integer function f(comptime integer p)
    begin if(p>0$i) ret(p+f(p-1$i)[1$i]); end_if; ret(p); end;
""", "pout(f(4$i)[1$i]);", [10])
case("comptime_loop_keeps_runtime_calls", "TP3 pp.5-6,8", """
    integer function identity(integer p) begin ret(p); end;
    comptime integer x;
    integer i;
""", "x := 0$i; from i := 0$i to 3$i by 1$i repeat begin "
     "x := x+1$i; pout(identity(i)[1$i]); end; pout(x);", [0, 1, 2, 3])
case("comptime_loop_body_changes_control", "TP3 pp.6,8", "comptime integer x; integer i;", """
    x := 0$i;
    from i := 0$i to 5$i by 1$i repeat begin
        x := x+1$i;
        if(i==1$i) i := 3$i; end_if;
    end;
    pout(x); pout(i);
""", [3, 5])
case("comptime_loop_call_changes_control", "TP3 pp.5-6,8", """
    integer i;
    comptime integer x;
    integer function stop(integer p) begin i := 5$i; ret(p); end;
""", "x := 0$i; from i := 0$i to 5$i by 1$i repeat begin "
     "x := x+1$i; pout(stop(i)[1$i]); end; pout(x); pout(i);", [0, 1, 6])
case("comptime_preserves_embedded_effect", "TP3 pp.7-8", "comptime integer x,y; integer a;", """
    x := a=3$i; pout(a); pout(x);
    a := x=y=4$i; pout(a); pout(x); pout(y);
    a := x=x+1$i; pout(a); pout(x);
""", [3, 3, 4, 4, 4, 5, 4])
case("comptime_call_runtime_effect", "TP3 pp.5,8", """
    comptime integer x;
    integer function f(integer p) begin pout(p); ret(5$i); end;
""", "x := f(2$i)[1$i]; pout(x);", [2, 5])
case("comptime_float_and_conversion", "TP3 pp.4,8", "comptime singlef x; comptime integer y;", """
    x := 1.5; x := x+2.25; y := toi(x); pout(x); pout(y);
""", [3.75, 3])
case("comptime_uninitialized", "TP3 p.8", "comptime integer x;", "pout(x);",
     error="has no known value", error_at="pout(x);")
case("comptime_integer_overflow", "TP3 p.8; topic 3 range", "comptime integer x;",
     "x := 32767$i+1$i;", error="not known at compile time")
case("comptime_division_by_zero", "TP3 p.8", "comptime integer x;",
     "x := 1$i/0$i;", error="not known at compile time")
case("comptime_empty_loop_under_guard", "TP3 pp.6,8", """
    integer function f(integer p)
    comptime integer x;
    integer i;
    begin
        x := 3$i;
        if(p>0$i) from i := 0$i to 0$i by 0$i repeat x := 4$i;; end_if;
        ret(x);
    end;
""", "pout(f(1$i)[1$i]); pout(f(0$i)[1$i]);", [3, 3])

case("structural_copy_renamed_fields", "TP3 p.9; confirmed type/order rule", """
    class a shared begin integer first; singlef second; end;
    class b shared begin integer other; singlef renamed; end;
    a source;
    b target;
""", "source[0$i] := 3$i; source[1$i] := 1.5; target := source; "
     "source[0$i] := 9$i; pout(target[0$i]); pout(target[1$i]);", [3, 1.5])
case("structural_methods_do_not_count", "TP3 p.9; confirmed type/order rule", """
    class a shared begin integer first; integer m(integer p) begin ret(p); end; end;
    class b shared begin integer other; singlef different(singlef p) begin ret(p); end; end;
    a source; b target;
""", "source[0$i] := 3$i; target := source; pout(target[0$i]);", [3])
for name, attributes in [
    ("structure_count_error", "integer x,y;"),
    ("structure_type_error", "singlef x;"),
]:
    case(name, "TP3 p.9", f"class a shared begin integer x; end;\n"
         f"class b shared begin {attributes} end;", "pout(1$i);", error="incompatible attribute types or order")
case("structure_order_error", "TP3 p.9; confirmed rule", """
    class a shared begin integer x; singlef y; end;
    class b shared begin singlef x; integer y; end;
""", "pout(1$i);", error="incompatible attribute types or order")
case("structure_code_error", "TP3 p.9", """
    class a firstcode begin integer x; end;
    class b secondcode begin integer y; end;
    a source; b target;
""", "target := source;", error="requires matching class codes")
case("structure_qualified_field_type", "TP3 pp.3,9; confirmed rule", """
    class kind outercode begin integer x; end;
    class first shared begin kind value; end;
    integer function f(integer p)
    class kind innercode begin integer x; end;
    class second shared begin kind value; end;
    begin ret(p); end;
""", "pout(f(1$i)[1$i]);", error="incompatible attribute types or order")
case("nested_object_copy", "TP3 pp.5,9", """
    class inner innercode begin integer value; end;
    class outer outercode begin inner item; end;
    inner source, changed, result;
    outer a,b;
""", "source[0$i] := 3$i; a[0$i] := source; b := a; changed[0$i] := 7$i; "
     "a[0$i] := changed; result := b[0$i]; pout(result[0$i]);", [3])

case("positional_constant_and_variable", "TP3 p.10; TP2 p.7", """
    class c code begin integer x,z; end;
    c object; integer index;
""", "object[0$i] := 3$i; index := 1$i; object[index] := 4$i; "
     "pout(object[0$i]); pout(object[index]);", [3, 4])
case("positional_runtime_parameter", "TP3 p.10", """
    class c code begin integer x,z; end;
    c object;
    integer function get(integer index) begin ret(object[index]); end;
""", "object[0$i] := 3$i; object[1$i] := 4$i; pout(get(0$i)[1$i]); pout(get(1$i)[1$i]);", [3, 4])
for name, index in [("position_negative", "-1$i"), ("position_outside", "2$i")]:
    case(name, "TP3 p.10", "class c code begin integer x,z; end; c object;",
         f"object[{index}] := 1$i;", error="does not exist")
case("position_after_declaration_only", "TP3 pp.3,10", """
    class c code begin
        integer first;
        integer read(c object) begin ret(object[1$i]); end;
        integer later;
    end;
""", "pout(1$i);", error="does not exist")

DIAMOND = """
    class base basecode begin
        integer value;
        integer read(integer p) begin ret(value+p); end;
    end;
    class left leftcode begin extends base; integer left_value; end;
    class right rightcode begin extends base; integer right_value; end;
    class derived shared begin extends left,right; integer own; end;
"""
case("diamond_identity_and_order", "TP3 pp.10-11; confirmed inheritance rules", DIAMOND + "derived object;", """
    object[0$i] := 3$i; object[1$i] := 4$i; object[2$i] := 5$i; object[3$i] := 6$i;
    pout(object.read(1$i)[1$i]);
    pout(object[0$i]); pout(object[1$i]); pout(object[2$i]); pout(object[3$i]);
""", [4, 3, 4, 5, 6], symbols={"derived:main": {"Effective attributes":
    "[value:main:base, left_value:main:left, right_value:main:right, own:main:derived]"}})
case("structural_effective_attributes", "TP3 pp.9-11; confirmed inheritance rules", DIAMOND + """
    class plain shared begin integer a,b,c,d; end;
    derived source; plain target;
""", "source[0$i] := 1$i; source[1$i] := 2$i; source[2$i] := 3$i; source[3$i] := 4$i; "
     "target := source; pout(target[0$i]); pout(target[3$i]);", [1, 4])
case("inherited_implicit_lookup", "TP3 p.11", """
    class base basecode begin
        integer value;
        integer read(integer p) begin ret(value+p); end;
    end;
    class derived derivedcode begin
        extends base;
        integer twice(integer p) begin ret(read(p)[1$i]+value); end;
    end;
    derived object;
""", "object[0$i] := 3$i; pout(object.twice(1$i)[1$i]);", [7])
case("own_method_precedes_bases", "TP3 p.11", """
    class a acode begin integer m(integer p) begin ret(1$i); end; end;
    class b bcode begin integer m(integer p) begin ret(2$i); end; end;
    class c ccode begin extends a,b; integer m(integer p) begin ret(3$i); end; end;
    c object;
""", "pout(object.m(0$i)[1$i]);", [3])
case("distinct_inherited_methods_ambiguous", "TP3 p.11", """
    class a acode begin integer m(integer p) begin ret(p); end; end;
    class b bcode begin integer m(integer p) begin ret(p); end; end;
    class c ccode begin extends a,b; integer x; end;
    c object;
""", "pout(object.m(1$i)[1$i]);", error="is ambiguous")
case("distinct_inherited_attributes_ambiguous", "TP3 pp.10-11", """
    class a acode begin integer x; end;
    class b bcode begin integer x; end;
    class c ccode begin extends a,b; integer own; end;
    c object;
""", "object[0$i] := 1$i;", error="is ambiguous")
case("inheritance_cycle", "TP3 p.11; finite schema requirement",
     "class c code begin extends c; integer x; end;", "pout(1$i);", error="Cyclic inheritance")

case("method_receiver_independence", "TP3 p.5; topic 29", """
    class c code begin
        integer value;
        integer put(integer p) begin value := p; ret(value); end;
        integer get(integer p) begin ret(value+p); end;
    end;
    c first,second;
""", "pout(first.put(3$i)[1$i]); pout(second.put(7$i)[1$i]); "
     "pout(first.get(second.get(1$i)[1$i])[1$i]); pout(first[0$i]);", [3, 7, 11, 3])
case("method_local_class_lexical_parent", "TP3 pp.2-3,5", """
    integer function outer(integer p)
    class c code begin integer m(integer q) begin ret(p+q); end; end;
    c object;
    begin ret(object.m(2$i)[1$i]); end;
""", "pout(outer(3$i)[1$i]);", [5])
case("comptime_method_bindings", "TP3 pp.5,8", """
    class c code begin
        integer value;
        integer add(comptime integer p) begin ret(value+p); end;
    end;
    c first,second;
""", "first[0$i] := 3$i; second[0$i] := 7$i; "
     "pout(first.add(1$i)[1$i]); pout(second.add(2$i)[1$i]); pout(first.add(1$i)[1$i]);", [4, 9, 4])

for name, declarations, body in [
    ("object_return_header", "class c code begin integer x; end;\n"
     "c function f(integer p) c object; begin ret(object); end;", "pout(1$i);"),
    ("object_return_value", "class c code begin integer x; end;\n"
     "integer function f(c object) begin ret(object); end;", "pout(1$i);"),
    ("object_method_return", "class c code begin integer x; c m(integer p) begin ret(p); end; end;", "pout(1$i);"),
]:
    case(name, "Confirmed primitive return rule; TP2 p.1", declarations, body, error="object returns|Object returns")

case("explicit_conversion_example", "TP3 p.4, topic 34", "integer a; singlef x,y;", """
    x := 1.5; y := 2.25; a := toi(x+y); pout(a);
""", [3], contains=["TOI"])
case("explicit_conversion_operand", "TP3 p.4, topic 34 adapted to INTEGER", "integer a,b; singlef x;",
     "x := 3.75; b := 2$i; a := toi(x)+b; pout(a);", [5], contains=["TOI"])
for name, expression in [("mixed_operands_deferred", "z+b"), ("float_assignment_deferred", "x+y")]:
    case(name, "TP3 p.4, invalid numeric examples deferred to TP4", "integer a,b; singlef x,y,z;",
         f"x := 1.0; y := 2.0; z := 3.0; b := 1$i; a := {expression};", execute=False,
         excludes=["TOI"], boundary="TP4 numeric compatibility")
case("loop_types_deferred", "TP3 pp.4,6, numeric checks deferred for RPN", "singlef i;",
     "from i := 1.0 to 2.0 by 1.0 repeat pout(i);;", execute=False, boundary="TP4 loop operand types")

case("output_literal_disambiguation", "TP3 pp.1,12", "integer a;", """
    a := 1$i;
    pout("BF"); pout("CALL f:main"); pout("3$i"); pout("a:main"); pout(a);
""", ["BF", "CALL f:main", "3$i", "a:main", 1])
case("independent_error_recovery", "TP3 p.12", "integer a;", """
    missing := 1$i;
    a := 2$i;
    pout();
    absent := 3$i;
""", error="not declared or not visible", diagnostics=["Missing argument in pout", "'absent'"])
case("ordinary_nonterminating_loop_compiles", "TP3 p.6; runtime/compile-time boundary", "integer i;",
     "from i := 0$i to 1$i by 0$i repeat pout(i);;", execute=False, contains=["BI", "BF"])
case("specialization_resource_guard", "Documented implementation resource limit", """
    integer function f(comptime integer p)
    begin if(p>0$i) ret(f(p-1$i)[1$i]); end_if; ret(p); end;
""", "pout(f(200$i)[1$i]);", error="recursion resource limit")

case("integer_division_consistency", "TP3 pp.1,8; INTEGER representation", """
    comptime integer value;
    integer function divide(integer p) begin ret(p/3$i/2$i); end;
""", "value := 20$i/3$i/2$i; pout(value); pout(divide(20$i)[1$i]); "
     "value := -7$i/2$i; pout(value);", [3, 3, -3])
case("comptime_float_parameters", "TP3 pp.5,8", """
    singlef function f(comptime singlef p) begin ret(p+1.5); end;
""", "pout(f(1.25)[1$i]); pout(f(2.5)[1$i]);", [2.75, 4.0])
case("comptime_mixed_formals", "TP3 pp.5,8", """
    integer function f(comptime integer p,integer q) begin ret(p+q); end;
""", "pout(f(3$i,1$i)[1$i,2$i]); pout(f(3$i,2$i)[2$i,1$i]); "
     "pout(f(7$i,2$i)[1$i,2$i]);", [4, 5, 9])
case("comptime_runtime_recursion", "TP3 pp.5,8", """
    integer function f(comptime integer p,integer n)
    begin if(n>0$i) ret(p+f(p,n-1$i)[1$i,2$i]); end_if; ret(0$i); end;
""", "pout(f(3$i,5$i)[1$i,2$i]);", [15])
case("ordinary_recursion_does_not_specialize", "TP3 p.5; runtime boundary", """
    integer function f(integer p) begin ret(f(p)[1$i]); end;
""", "pout(f(1$i)[1$i]);", execute=False)
case("comptime_method_lexical_capture", "TP3 pp.2-3,5,8", """
    integer function outer(comptime integer p)
    class local code begin integer read(integer q) begin ret(p+q); end; end;
    local object;
    begin ret(object.read(2$i)[1$i]); end;
""", "pout(outer(3$i)[1$i]); pout(outer(7$i)[1$i]);", [5, 9])
case("comptime_attribute_instances", "TP3 pp.8,10; instance isolation", """
    class c code begin
        comptime integer value;
        integer read(integer p) begin ret(value+p); end;
    end;
    c first,second;
""", "first[0$i] := 3$i; second[0$i] := 7$i; "
     "pout(first.read(1$i)[1$i]); pout(second.read(1$i)[1$i]); pout(first.read(2$i)[1$i]);", [4, 8, 5])
case("comptime_attribute_dynamic_position", "TP3 pp.8,10", """
    class c code begin comptime integer value; integer other; end;
    c object;
    integer function f(integer index) begin ret(object[index]); end;
""", "object[0$i] := 3$i; object[1$i] := 7$i; pout(f(0$i)[1$i]);",
     error="position must be known")
case("comptime_index_substitution", "TP3 pp.8,10", """
    class c code begin integer first,second; end;
    c object;
    comptime integer index;
""", "index := 1$i; object[index] := 7$i; pout(object[index]);", [7])
case("comptime_negative_index", "TP3 pp.8,10", """
    class c code begin integer value; end;
    c object;
    comptime integer index;
""", "index := -1$i; object[index] := 7$i;", error="does not exist")
case("comptime_runtime_index_invalidates_fields", "TP3 pp.8,10", """
    class c code begin integer first,second; end;
    c object;
    comptime integer x;
    integer function index(integer p) begin ret(p); end;
    integer position;
""", "object[0$i] := 3$i; object[1$i] := 7$i; position := index(0$i)[1$i]; "
     "object[position] := 9$i; x := object[0$i];", error="not known at compile time")
case("comptime_object_copy_refreshes_fields", "TP3 pp.8-10", """
    class c code begin integer value; end;
    c first,second;
    comptime integer x;
""", "first[0$i] := 3$i; second[0$i] := 7$i; first := second; x := first[0$i]; pout(x);", [7])
case("runtime_object_copy_invalidates_fields", "TP3 pp.5,8-10", """
    class c code begin integer value; end;
    c first,second;
    comptime integer x;
    integer function change(integer p) begin first := second; ret(p); end;
""", "first[0$i] := 3$i; second[0$i] := 7$i; pout(change(1$i)[1$i]); x := first[0$i];",
     error="not known at compile time")
case("comptime_loop_runtime_branch_join", "TP3 pp.6,8", """
    comptime integer x;
    integer function f(integer p)
    integer i;
    begin
        from i := 0$i to 3$i by 1$i repeat begin
            x := x+1$i;
            if(p>0$i) pout(1$i); else pout(2$i); end_if;
        end;
        ret(x);
    end;
""", "x := 0$i; pout(f(0$i)[1$i]); pout(x);", [2, 2, 2, 3, 3])
case("comptime_return_inside_expanded_loop", "TP3 pp.5-6,8", """
    integer function f(comptime integer p)
    comptime integer x;
    begin
        from x := 0$i to 10$i by 1$i repeat begin
            if(x==p) ret(x); end_if;
        end;
        ret(-1$i);
    end;
""", "pout(f(3$i)[1$i]); pout(f(7$i)[1$i]);", [3, 7])
case("large_parameter_list", "TP3 pp.1,5,7; dynamic storage",
     "integer function f(" + ",".join(f"integer p{i}" for i in range(1, 81)) +
     ") begin ret(p1-p80); end;",
     "pout(f(" + ",".join(f"{i}$i" for i in range(1, 81)) + ")[" +
     ",".join(f"{i}$i" for i in range(80, 0, -1)) + "]);", [-79])

case("nested_noncommutative_copy", "TP3 pp.5,7", """
    integer function f(integer first,integer second) begin ret(10$i*first+second); end;
""", "pout(f(1$i,f(2$i,3$i)[1$i,2$i])[1$i,2$i]);", [33])
case("operand_snapshot_before_call", "TP3 pp.5,7", """
    integer a;
    integer function f(integer p) begin ret(p); end;
""", "a := 1$i; pout(a+f(a=2$i)[1$i]); pout(a);", [3, 2])
case("comptime_operand_snapshot", "TP3 pp.5,7-8", """
    comptime integer x,y;
    integer function f(integer p) begin pout(p); ret(5$i); end;
""", "x := 1$i; y := x+f(x=2$i)[1$i]; pout(y); pout(x);", [2, 6, 2])
case("comptime_external_mutation_calls", "TP3 pp.5,8", """
    comptime integer x;
    integer function increment(integer p) begin x := x+1$i; ret(x); end;
""", "x := 0$i; pout(increment(0$i)[1$i]); pout(increment(0$i)[1$i]); pout(x);", [1, 2, 2])
case("comptime_loop_external_mutation_calls", "TP3 pp.5-6,8", """
    comptime integer x;
    integer i;
    integer function increment(integer p) begin x := x+1$i; ret(x); end;
""", "x := 0$i; from i := 0$i to 3$i by 1$i repeat pout(increment(i)[1$i]);; pout(x);", [1, 2, 3, 3])
case("staged_function_keeps_runtime_global", "TP3 pp.5,8", """
    integer value;
    integer function f(comptime integer p) begin ret(value+p); end;
""", "value := 1$i; pout(f(3$i)[1$i]); value := 7$i; pout(f(3$i)[1$i]);", [4, 10])
case("comptime_local_does_not_restore_global", "TP3 pp.5,8", """
    comptime integer x;
    integer function f(integer p)
    comptime integer local;
    begin local := 3$i; ret(local+p); end;
""", "x := 1$i; pout(f(1$i)[1$i]); x := 2$i; pout(f(1$i)[1$i]); pout(x);", [4, 4, 2])
case("comptime_read_only_known_dead_branch", "TP3 p.8", "comptime integer x,y;", """
    x := 0$i;
    if(x>0$i) pout(y); else pout(x); end_if;
""", [0])
case("comptime_runtime_helper_under_guard", "TP3 pp.5,8", """
    integer function helper(integer p)
    comptime integer local;
    begin local := 3$i; ret(local+p); end;
    integer function f(integer p)
    begin if(p>0$i) ret(helper(p)[1$i]); end_if; ret(0$i); end;
""", "pout(f(1$i)[1$i]); pout(f(0$i)[1$i]);", [4, 0])
case("own_attribute_and_base_storage", "TP3 pp.10-11; confirmed effective layout", """
    class base basecode begin
        integer value;
        integer base_read(integer p) begin ret(value+p); end;
    end;
    class derived derivedcode begin
        extends base;
        integer value;
        integer own_read(integer p) begin ret(value+p); end;
    end;
    derived object;
""", "object[0$i] := 3$i; object[1$i] := 7$i; "
     "pout(object.base_read(0$i)[1$i]); pout(object.own_read(0$i)[1$i]);", [3, 7])
case("mixed_inherited_layout_and_copy", "TP3 pp.9-11; confirmed base order", """
    class a acode begin integer first; end;
    class b bcode begin singlef second; end;
    class derived shared begin extends a,b; integer own; end;
    class plain shared begin integer x; singlef y; integer z; end;
    derived source; plain target;
""", "source[0$i] := 3$i; source[1$i] := 1.5; source[2$i] := 7$i; "
     "target := source; pout(target[0$i]); pout(target[1$i]); pout(target[2$i]);", [3, 1.5, 7])
case("mixed_inherited_order_error", "TP3 pp.9-11; confirmed base order", """
    class a acode begin integer first; end;
    class b bcode begin singlef second; end;
    class derived shared begin extends b,a; integer own; end;
    class plain shared begin integer x; singlef y; integer z; end;
""", "pout(1$i);", error="incompatible attribute types or order")
case("literal_symbol_category_collision", "TP3 pp.1,12", """
    integer function literals(integer p) begin pout("a:main"); pout("1.5"); ret(p); end;
    integer a;
    singlef value;
""", "a := 3$i; value := 1.5; pout(literals(a)[1$i]); pout(a); pout(value);",
     ["a:main", "1.5", 3, 3, 1.5], symbols={"a:main": {"Use": "VARIABLE"}, "1.5": {"Type": "SINGLEF"}})
case("multiline_chain_output", "TP3 pp.1,12; topic 10", "integer a;",
     'a := 1$i; pout("first\nsecond"); pout(a);', ["firstsecond", 1])
case("parameter_types_deferred", "TP3 pp.4-5", "integer function f(integer p) begin ret(p); end;",
     "pout(f(1.5)[1$i]);", execute=False, boundary="TP4 parameter compatibility")
case("return_types_deferred", "TP3 pp.4-5", "integer function f(integer p) begin ret(1.5); end;",
     "pout(f(1$i)[1$i]);", execute=False, boundary="TP4 numeric return compatibility")

case("comptime_indexed_write_effect", "TP3 pp.5,8,10", """
    class c code begin comptime integer value; end;
    c object;
    integer function set(comptime integer index,comptime integer value)
    begin object[index] := value; ret(object[index]); end;
""", "object[0$i] := 3$i; pout(set(0$i,7$i)[1$i,2$i]); pout(object[0$i]);", [7, 7])
case("comptime_indexed_write_in_loop", "TP3 pp.5-6,8,10", """
    class c code begin comptime integer value; end;
    c object; integer i;
    integer function set(comptime integer index,comptime integer value)
    begin object[index] := value; ret(object[index]); end;
""", "object[0$i] := 3$i; from i := 0$i to 2$i by 1$i repeat pout(set(0$i,7$i)[1$i,2$i]);; "
     "pout(object[0$i]);", [7, 7, 7])
case("cached_indexed_write_respects_guard", "TP3 pp.5,8,10; confirmed control rule", """
    class c code begin comptime integer value; end;
    c object;
    integer function set(comptime integer index,comptime integer value)
    begin object[index] := value; ret(object[index]); end;
    integer function f(integer p)
    begin if(p>0$i) ret(set(0$i,7$i)[1$i,2$i]); end_if; ret(0$i); end;
""", "object[0$i] := 3$i; pout(set(0$i,7$i)[1$i,2$i]); "
     "pout(set(0$i,7$i)[1$i,2$i]); pout(f(0$i)[1$i]);", error="under unknown control")
case("object_copy_propagates_comptime_fields", "TP3 pp.5,8-10", """
    class c code begin comptime integer value; end;
    c first,second;
    integer function copy(integer p) begin first := second; ret(p); end;
""", "first[0$i] := 7$i; second[0$i] := 3$i; pout(copy(0$i)[1$i]); pout(first[0$i]);", [0, 3])

for order, expected in [("1$i,2$i", 3), ("2$i,1$i", 7)]:
    case("object_parameter_comptime_snapshot_" + order[0], "TP3 pp.5,7-10", """
        class c code begin comptime integer value; end;
        c object;
        integer function change(comptime integer value) begin object[0$i] := value; ret(value); end;
        integer function read(c copy,integer ignored) begin ret(copy[0$i]); end;
    """, "object[0$i] := 3$i; pout(read(object,change(7$i)[1$i])[" + order + "]); "
         "pout(object[0$i]);", [expected, 7])
case("structural_copy_comptime_to_runtime", "TP3 pp.8-9; confirmed type/order rule", """
    class a shared begin comptime integer value; end;
    class b shared begin integer value; end;
    a source; b target;
""", "source[0$i] := 3$i; target := source; pout(target[0$i]);", [3])
case("structural_copy_runtime_to_comptime", "TP3 pp.8-9; confirmed type/order rule", """
    class a shared begin integer value; end;
    class b shared begin comptime integer value; end;
    a source; b target;
""", "source[0$i] := 3$i; target := source; pout(target[0$i]);", [3])

case("object_parameter_local_comptime_state", "TP3 pp.5,8-10; per-activation isolation", """
    class c code begin comptime integer value; end;
    c object;
    integer function change(c copy) begin copy[0$i] := 7$i; ret(copy[0$i]); end;
    integer function f(integer p)
    integer i;
    begin
        from i := 0$i to 2$i by 1$i repeat begin
            if(p>0$i) pout(change(object)[1$i]); end_if;
        end;
        ret(0$i);
    end;
""", "object[0$i] := 3$i; pout(f(1$i)[1$i]); pout(object[0$i]);", [7, 7, 0, 3])
for source_modifier, formal_modifier in [("comptime ", ""), ("", "comptime ")]:
    case("object_parameter_qualifier_" + ("source" if source_modifier else "formal"),
         "TP3 pp.5,8-9; confirmed type/order rule",
         "class a shared begin " + source_modifier + "integer value; end;\n"
         "class b shared begin " + formal_modifier + "integer renamed; end;\n"
         "a object;\ninteger function f(b copy) begin ret(copy[0$i]); end;",
         "object[0$i] := 3$i; pout(f(object)[1$i]);", [3])

case("singlef_compiletime_runtime_rounding", "TP3 pp.1,8; topic 7 representation",
     "comptime singlef staged; singlef runtime;",
     "staged := 0.1+0.2; runtime := 0.1+0.2; pout(staged); pout(runtime);",
     [0.30000001192092896, 0.30000001192092896])
case("expansion_resource_guard", "Documented implementation resource limit", "comptime integer x; integer i;",
     "x := 0$i; from i := 0$i to 1$i by 0$i repeat x := 0$i;;",
     error="loop evaluation exceeded the resource limit")

case("from_negative_step_ascending", "TP3 p.6; update uses the body's current control value", "integer i;",
     "from i := 0$i to 2$i by -1$i repeat begin pout(i); i := i+2$i; end;", [0, 1])
case("from_negative_step_descending", "TP3 p.6; update uses the body's current control value", "integer i;",
     "from i := 2$i to 0$i by -1$i repeat begin pout(i); i := i-2$i; end;", [2, 1])
case("from_zero_step_with_body_progress", "TP3 p.6; no extra BY sign restriction", "integer i;",
     "from i := 0$i to 2$i by 0$i repeat begin pout(i); i := i+1$i; end;", [0, 1])
