"""TP4 expectations, derived from source semantics rather than the emitter."""
CASES = []


def case(name, declarations, statements, output=(), compile_error=None, runtime_error=None):
    CASES.append(dict(name=name, source=f'review\n{declarations}\nbegin\n{statements}\nend;\n',
                      output=list(output), compile_error=compile_error, runtime_error=runtime_error))

case('integer_limits', 'integer x;', 'x := -32768$i; pout(x); x := 32767$i; pout(x);', [-32768, 32767])
case('integer_wrap', 'integer x;', 'x := 32767$i + 1$i; pout(x); x := -32768$i - 1$i; pout(x);', [-32768, 32767])
case('integer_wrap_comptime', 'comptime integer x;', 'x := 32767$i + 1$i; pout(x);', [-32768])
case('division_truncation', 'integer x;', 'x := -7$i / 2$i; pout(x); x := -32768$i / -1$i; pout(x);', [-3, -32768])
case('product_valid', 'integer x;', 'x := -128$i * 255$i; pout(x);', [-32640])
for name, expression in [('positive', '200$i*200$i'), ('negative', '-32768$i*2$i'), ('minimum', '-32768$i*-1$i')]:
    case('product_' + name, 'integer x;', f'pout(9$i); x := {expression}; pout(99$i);', [9], runtime_error='(d)')
case('product_from_call', 'integer function f(integer p) begin ret(p*p); end;', 'pout(f(200$i)[1$i]); pout(99$i);', runtime_error='(d)')
case('product_comptime_error', 'comptime integer x;', 'x := 200$i * 200$i;', compile_error='multiplication overflow')
case('float_add_valid', 'singlef x;', 'x := 1.25+2.5; pout(x);', [3.75])
for name, expression in [('positive', '3.0s38+3.0s38'), ('negative', '-3.0s38+-3.0s38')]:
    case('float_overflow_' + name, 'singlef x;', f'pout(9$i); x := {expression}; pout(99$i);', [9], runtime_error='(c)')
case('float_from_call', 'singlef function f(singlef p) begin ret(p+p); end;', 'pout(f(3.0s38)[1$i]); pout(99$i);', runtime_error='(c)')
case('float_comptime_error', 'comptime singlef x;', 'x := 3.0s38+3.0s38;', compile_error='addition overflow')
case('float_rounding', 'singlef x;', 'x := 16777216.0+1.0; pout(x);', [16777216])
case('conversion_exact', 'integer x;', 'x := toi(3.0); pout(x); x := toi(-32768.0); pout(x); x := toi(32767.0); pout(x); x := toi(-0.0); pout(x);', [3,-32768,32767,0])
for name, value in [('fraction', '3.9'), ('negative_fraction', '-3.9')]:
    case('conversion_' + name, 'integer x;', f'pout(9$i); x := toi({value}); pout(99$i);', [9], runtime_error='(g)')
for name, value in [('large', '32768.0'), ('small', '-32769.0')]:
    case('conversion_' + name, 'integer x;', f'x := toi({value}); pout(99$i);', runtime_error='not representable')
case('conversion_nan', 'integer x;', 'x := toi(0.0/0.0); pout(99$i);', runtime_error='not representable')
case('conversion_infinity', 'integer x;', 'x := toi(1.0/0.0); pout(99$i);', runtime_error='not representable')
case('conversion_condition_effect', 'integer x;', 'if(toi(3.9)==3$i) pout(99$i); end_if;', runtime_error='(g)')
case('conversion_comptime_error', 'comptime integer x;', 'x := toi(3.9);', compile_error='fractional information')
case('conversion_type_error', 'integer x;', 'x := toi(3$i);', compile_error='TOI requires SINGLEF')
case('dead_branch_types', 'integer x;', 'if(1$i<2$i) x := 1$i; else x := 1.5; end_if;', compile_error='Incompatible assignment')
case('unused_function_types', 'integer function f(integer p) begin ret(1.5); end;', 'pout(1$i);', compile_error='return type')
case('after_return_types', 'integer function f(integer p) begin ret(p); ret(1.5); end;', 'pout(1$i);', compile_error='return type')
case('unused_template_types', 'integer function f(comptime integer p) begin ret(1.5); end;', 'pout(1$i);', compile_error='return type')
case('recursion_direct', 'integer function f(integer p) begin ret(f(p)[1$i]); end;', 'pout(9$i); pout(f(1$i)[1$i]); pout(99$i);', [9], runtime_error='(i)')
case('recursion_indirect', '''integer function outer(integer p)
integer function inner(integer q) begin ret(outer(q)[1$i]); end;
begin ret(inner(p)[1$i]); end;''', 'pout(outer(1$i)[1$i]);', runtime_error='(i)')
case('calls_successive_nested', 'integer function f(integer p) begin ret(p+1$i); end;', 'pout(f(f(1$i)[1$i])[1$i]); pout(f(3$i)[1$i]);', [3,4])
case('early_returns', 'integer function f(integer p) begin if(p>0$i) ret(7$i); end_if; ret(3$i); end;', 'pout(f(1$i)[1$i]); pout(f(0$i)[1$i]); pout(f(2$i)[1$i]);', [7,3,7])
case('method_reentry_receiver', '''class box code begin integer value;
integer f(box other) begin ret(other.f(other)[1$i]); end; end;
box a,b;''', 'pout(a.f(b)[1$i]);', runtime_error='(i)')
case('inherited_second_base', '''class first a begin integer x; end;
class second b begin integer y; integer get(integer ignored) begin ret(y); end; end;
class derived c begin extends first,second; integer z; end;
second base; derived object;''', 'base[0$i] := 7$i; object[0$i] := 11$i; object[1$i] := 22$i; object[2$i] := 33$i; pout(base.get(0$i)[1$i]); pout(object.get(0$i)[1$i]); pout(object[0$i]);', [7,22,11])
case('variable_index_integer', '''class pair code begin integer a,b; end;
pair object; integer function read(integer index) begin ret(object[index]+1$i); end;''', 'object[0$i] := 3$i; object[1$i] := 7$i; pout(read(0$i)[1$i]); pout(read(1$i)[1$i]);', [4,8])
case('variable_index_mixed_print', '''class pair code begin integer a; singlef b; end;
pair object; integer function show(integer index) begin pout(object[index]); ret(0$i); end;''', 'object[0$i] := 3$i; object[1$i] := 1.5; pout(show(0$i)[1$i]); pout(show(1$i)[1$i]);', [3,0,1.5,0])
case('variable_index_mixed_reject', '''class pair code begin integer a; singlef b; end;
pair object; integer function read(integer index) begin ret(object[index]+1$i); end;''', 'pout(0$i);', compile_error='Incompatible operand')
for name, value in [('negative','-1$i'),('upper','2$i')]:
    case('index_' + name, '''class pair code begin integer a,b; end;
pair object; integer function read(integer index) begin ret(object[index]); end;''', f'pout(read({value})[1$i]); pout(99$i);', runtime_error='Attribute index')
case('comptime_index_deferred', '''class pair code begin integer a; singlef b; end;
pair object; integer function read(comptime integer index) begin ret(object[index]); end;''', 'object[0$i] := 7$i; pout(read(0$i)[1$i]);', [7])
case('comptime_index_bad_specialization', '''class pair code begin integer a; singlef b; end;
pair object; integer function read(comptime integer index) begin ret(object[index]); end;''', 'object[1$i] := 1.5; pout(read(1$i)[1$i]);', compile_error='return type')
case('comptime_index_unused', '''class pair code begin integer a; singlef b; end;
pair object; integer function read(comptime integer index) begin ret(object[index]); end;''', 'pout(7$i);', [7])
case('object_recursive', 'class node code begin node child; end; integer x;', 'x := 1$i;', compile_error='Recursive object containment')
case('nested_class_parameter_not_containment', '''class box code begin integer value;
integer read(box other) begin ret(other[0$i]); end; end; box object;''', 'object[0$i] := 3$i; pout(object.read(object)[1$i]);', [3])
case('operand_snapshot', '''integer x;
integer function change(integer p) begin x := p; ret(p); end;''', 'x := 3$i; pout(x+change(7$i)[1$i]); pout(x);', [10,7])
case('negative_condition', 'integer x;', 'x := 1$i; if(x>0$i) x := 2$i; else x := 3$i; end_if; pout(x);', [2])
case('destination_index_snapshot', '''class pair code begin integer a,b; end;
pair object; integer index;
integer function change(integer ignored) begin index := 1$i; ret(9$i); end;''', 'object[0$i] := 3$i; object[1$i] := 7$i; index := 0$i; object[index] := change(0$i)[1$i]; pout(object[0$i]); pout(object[1$i]);', [9,7])
case('variable_index_comptime_read', '''class pair code begin comptime integer a; integer b; end;
pair object; integer function read(integer index) begin ret(object[index]); end;''', 'object[0$i] := 3$i; object[1$i] := 7$i; pout(read(0$i)[1$i]); pout(read(1$i)[1$i]);', [3,7])
case('variable_index_comptime_write', '''class pair code begin comptime integer a; integer b; end;
pair object; integer index;''', 'object[0$i] := 3$i; index := 0$i; object[index] := 9$i;', compile_error='compile-time index')
case('variable_object_copy', '''class first shared begin integer value; end;
class second shared begin integer other; end;
class container code begin first left; second right; end;
first a; second b; container object;
integer function read(integer index) first local; begin local := object[index]; ret(local[0$i]); end;''',
'a[0$i] := 3$i; b[0$i] := 7$i; object[0$i] := a; object[1$i] := b; pout(read(0$i)[1$i]); pout(read(1$i)[1$i]);', [3,7])
case('variable_object_destination', '''class first shared begin integer value; end;
class second shared begin integer other; end;
class container code begin first left; second right; end;
first a; container object;
integer function put(integer index) begin object[index] := a; ret(0$i); end;
integer function read(integer index) first local; begin local := object[index]; ret(local[0$i]); end;''',
'a[0$i] := 9$i; pout(put(1$i)[1$i]); pout(read(1$i)[1$i]);', [0,9])
case('lexical_parent_not_caller', '''integer function outer(integer p)
integer value;
integer function read(integer ignored) begin ret(value); end;
integer function caller(integer ignored) integer value; begin value := 7$i; ret(read(0$i)[1$i]); end;
begin value := p; ret(caller(0$i)[1$i]); end;''', 'pout(outer(3$i)[1$i]); pout(outer(9$i)[1$i]);', [3,9])
case('comptime_calls_nested', 'integer function f(comptime integer p) begin ret(p+1$i); end;', 'pout(f(f(1$i)[1$i])[1$i]); pout(f(3$i)[1$i]);', [3,4])
case('comptime_read_snapshot_versions', '''class pair code begin comptime integer a; integer b; end;
pair object; integer function read(integer index) begin ret(object[index]); end;''',
'object[0$i] := 3$i; object[1$i] := 11$i; pout(read(0$i)[1$i]); object[0$i] := 7$i; pout(read(0$i)[1$i]); object[0$i] := 3$i; pout(read(0$i)[1$i]);', [3,7,3])
case('known_variable_index_invalid_runtime', '''class pair code begin integer a,b; end;
pair object; integer index;''', 'index := -1$i; pout(9$i); pout(object[index]); pout(99$i);', [9], runtime_error='Attribute index')
case('invalid_destination_before_rhs', '''class pair code begin integer a,b; end;
pair object; integer index;
integer function change(integer ignored) begin pout(88$i); ret(9$i); end;''', 'index := 2$i; pout(7$i); object[index] := change(0$i)[1$i]; pout(99$i);', [7], runtime_error='Attribute index')

# Instantiated type obligations survive pruning and early returns.
PAIR = 'class pair code begin integer a; singlef b; end; pair object;\n'
for name, body in [
    ('false_branch', 'if(1$i>2$i) ret(object[index]); end_if; ret(0$i);'),
    ('else_branch', 'if(1$i<2$i) ret(0$i); else ret(object[index]); end_if;'),
    ('after_return', 'ret(0$i); ret(object[index]);'),
    ('after_returning_if', 'if(index>0$i) ret(0$i); end_if; ret(object[index]);'),
    ('empty_loop', 'from i := 1$i to 1$i by 1$i repeat ret(object[index]);; ret(0$i);'),
]:
    declarations = PAIR + 'integer function read(comptime integer index) integer i; begin ' + body + ' end;'
    case('discarded_return_' + name, declarations, 'pout(read(1$i)[1$i]);', compile_error='return type: SINGLEF')
    if name != 'after_returning_if':
        case('discarded_valid_' + name, declarations, 'pout(read(0$i)[1$i]);', [0])
case('discarded_assignment', PAIR + '''integer function read(comptime integer index)
integer result; begin ret(0$i); result := object[index]; end;''',
'pout(read(1$i)[1$i]);', compile_error='Incompatible assignment types: INTEGER := SINGLEF')
case('discarded_binary', PAIR + '''integer function read(comptime integer index)
begin ret(0$i); ret(object[index]+1$i); end;''',
'pout(read(1$i)[1$i]);', compile_error='Incompatible operand types: SINGLEF and INTEGER')
case('discarded_conversion', PAIR + '''integer function read(comptime integer index)
begin ret(0$i); ret(toi(object[index])); end;''',
'pout(read(0$i)[1$i]);', compile_error='TOI requires SINGLEF')
case('discarded_argument', PAIR + '''integer function use(integer p) begin ret(p); end;
integer function read(comptime integer index) begin ret(0$i); pout(use(object[index])[1$i]); end;''',
'pout(read(1$i)[1$i]);', compile_error='Incompatible assignment types: INTEGER := SINGLEF')
for name, position, error in [('integer',0,None),('singlef',1,'return type: SINGLEF')]:
    case('discarded_nested_local_' + name, PAIR + '''integer function read(comptime integer index)
comptime integer selected;
begin if(1$i>2$i) begin
    if(index==0$i) selected := 0$i; else selected := 1$i; end_if;
    ret(object[selected]);
end end_if; ret(0$i); end;''', f'pout(read({position}$i)[1$i]);',
[0] if error is None else [], compile_error=error)
case('discarded_local_assignment', PAIR + '''integer function read(comptime integer index)
comptime integer selected;
begin if(index==0$i) begin selected := 1$i; ret(object[selected]); end end_if; ret(0$i); end;''',
'pout(read(1$i)[1$i]);', compile_error='return type: SINGLEF')
case('discarded_after_call_effect', PAIR + '''comptime integer selected;
integer function choose(comptime integer p) begin selected := p; ret(0$i); end;
integer function read(comptime integer index)
begin pout(choose(index)[1$i]); ret(0$i); ret(object[selected]); end;''',
'pout(read(1$i)[1$i]);', compile_error='return type: SINGLEF')
case('discarded_cached_context', PAIR + '''integer function read(comptime integer index)
begin ret(0$i); ret(object[index]); end;''',
'pout(read(0$i)[1$i]); pout(read(1$i)[1$i]);', compile_error='return type: SINGLEF')
for direction in ['-', '+']:
    case('recursive_comptime_change_' + ('decrease' if direction == '-' else 'increase'),
         'integer function f(comptime integer p) begin pout(p); ret(f(p'+direction+'1$i)[1$i]); end;',
         'pout(f(200$i)[1$i]); pout(999$i);', [200], runtime_error='(i)')
case('recursive_comptime_base_case', '''integer function f(comptime integer p)
begin if(p>0$i) ret(f(p-1$i)[1$i]); end_if; ret(0$i); end;''',
'pout(f(0$i)[1$i]); pout(f(f(0$i)[1$i])[1$i]);', [0,0])
case('recursive_comptime_indirect_change', '''integer function outer(comptime integer p)
integer function inner(comptime integer q) begin pout(2$i); ret(outer(q+1$i)[1$i]); end;
begin pout(1$i); ret(inner(p)[1$i]); end;''',
'pout(outer(200$i)[1$i]); pout(999$i);', [1,2], runtime_error='(i)')
case('discarded_pure_call_preserves_global_index', PAIR + '''comptime integer selected;
integer function pure(integer p) begin ret(p); end;
integer function read(comptime integer index)
begin if(1$i>2$i) begin pout(pure(index)[1$i]); ret(object[selected]); end end_if; ret(0$i); end;''',
'selected := 0$i; pout(read(1$i)[1$i]);', [0])
case('discarded_negative_index', PAIR + '''integer function read(comptime integer index)
begin ret(0$i); ret(object[index]); end;''',
'pout(read(-1$i)[1$i]);', compile_error='does not exist')
case('discarded_outside_index', PAIR + '''integer function read(comptime integer index)
begin ret(0$i); ret(object[index]); end;''',
'pout(read(2$i)[1$i]);', compile_error='does not exist')

for name, position, error in [('integer',0,None),('singlef',1,'return type: SINGLEF')]:
    case('discarded_call_result_' + name, PAIR + '''
integer function identity(comptime integer p) begin ret(p); end;
integer function read(comptime integer index)
comptime integer selected;
begin ret(0$i); selected := identity(index)[1$i]; ret(object[selected]); end;''',
f'pout(read({position}$i)[1$i]);', [0] if error is None else [], compile_error=error)
    case('discarded_call_effect_' + name, PAIR + '''comptime integer selected;
integer function choose(comptime integer p) begin selected := p; ret(0$i); end;
integer function read(comptime integer index)
begin ret(0$i); pout(choose(index)[1$i]); ret(object[selected]); end;''',
f'selected := 1$i; pout(read({position}$i)[1$i]); pout(selected);', [0,1] if error is None else [], compile_error=error)
    case('discarded_object_copy_' + name, PAIR + '''
class selector other begin comptime integer index; end;
selector original,copy;
integer function read(comptime integer index)
comptime integer selected;
begin ret(0$i); original[0$i] := index; copy := original;
selected := copy[0$i]; ret(object[selected]); end;''',
f'original[0$i] := {1-position}$i; copy[0$i] := {1-position}$i; '
f'pout(read({position}$i)[1$i]); pout(original[0$i]); pout(copy[0$i]);',
[0,1,1] if error is None else [], compile_error=error)
    case('discarded_object_argument_' + name, PAIR + '''
class selector other begin comptime integer index; end;
selector original;
integer function identity(selector item) begin ret(item[0$i]); end;
integer function read(comptime integer index)
comptime integer selected;
begin ret(0$i); original[0$i] := index; selected := identity(original)[1$i];
ret(object[selected]); end;''',
f'original[0$i] := {1-position}$i; pout(read({position}$i)[1$i]); pout(original[0$i]);',
[0,1] if error is None else [], compile_error=error)
case('discarded_unrelated_call_write', PAIR + '''comptime integer selected;
integer flag;
integer function change(integer p) begin flag := p; ret(p); end;
integer function read(comptime integer index)
begin ret(0$i); pout(change(index)[1$i]); ret(object[selected]); end;''',
'selected := 0$i; flag := 7$i; pout(read(0$i)[1$i]); pout(flag);', [0,7])

for context, body in [
    ('active', 'ret(object[index]);'),
    ('after_return', 'ret(0$i); ret(object[index]);'),
    ('false_branch', 'if(1$i>2$i) ret(object[index]); end_if; ret(0$i);'),
]:
    for position in (0, 1):
        case(f'method_attribute_index_{context}_{position}', PAIR + '''
class selector other begin
    comptime integer index;
    integer read(integer unused) begin ''' + body + ''' end;
end;
selector pick;''',
f'object[0$i] := 7$i; pick[0$i] := {position}$i; pout(pick.read(0$i)[1$i]);',
[7 if context == 'active' else 0] if position == 0 else [],
compile_error='return type: SINGLEF' if position else None)

for name, fields, start, stop, increment in [
    ('ascending', 'integer a; singlef b;', 0, 2, 1),
    ('descending', 'singlef a; integer b;', 1, -1, 1),
    ('third_iteration', 'integer a,b; singlef c;', 0, 3, 1),
    ('valid', 'integer a,b;', 0, 2, 1),
]:
    case('discarded_loop_' + name, f'class pair code begin {fields} end; pair object;\n' + f'''
integer function read(comptime integer unused)
comptime integer index;
integer result;
begin
    ret(0$i);
    from index := {start}$i to {stop}$i by {increment}$i repeat begin
        result := object[index];
    end;
end;''', 'pout(read(0$i)[1$i]);', [0] if name == 'valid' else [],
compile_error=None if name == 'valid' else 'Incompatible assignment types: INTEGER := SINGLEF')

case('discarded_loop_final_index', PAIR + '''integer function read(comptime integer unused)
comptime integer index;
begin ret(0$i);
    from index := 0$i to 1$i by 1$i repeat begin pout(index); end;
    ret(object[index]);
end;''', 'pout(read(0$i)[1$i]);', compile_error='return type: SINGLEF')

case('discarded_loop_nested', '''class triple code begin integer a,b; singlef c; end;
triple object;
integer function read(comptime integer unused)
comptime integer i,j,index;
integer result;
begin ret(0$i);
    from i := 0$i to 2$i by 1$i repeat begin
        from j := 0$i to 2$i by 1$i repeat begin
            index := i+j;
            result := object[index];
        end;
    end;
end;''', 'pout(read(0$i)[1$i]);', compile_error='Incompatible assignment types: INTEGER := SINGLEF')

case('discarded_loop_snapshots', '''class selector code begin comptime integer index; end;
selector original,copy;
comptime integer index;
integer function read(comptime integer unused)
begin ret(0$i);
    from index := 0$i to 128$i by 1$i repeat begin
        original[0$i] := index;
        copy := original;
    end;
end;''',
'original[0$i] := 3$i; copy[0$i] := 7$i; index := 9$i; pout(read(0$i)[1$i]); '
'pout(original[0$i]); pout(copy[0$i]); pout(index);', [0,3,7,9])

case('discarded_loop_stable', PAIR + '''integer function read(comptime integer unused)
comptime integer index;
integer result;
begin ret(0$i);
    from index := 0$i to 1$i by 0$i repeat begin result := object[index]; end;
end;''', 'pout(read(0$i)[1$i]);', [0])

case('discarded_loop_runtime_large', '''integer function read(integer unused)
integer i,j;
begin ret(0$i);
    from i := 0$i to 1000$i by 1$i repeat begin
        from j := 0$i to 1000$i by 1$i repeat begin pout(j); end;
    end;
end;''', 'pout(read(0$i)[1$i]);', [0])


case('recursive_probe_in_loop',
     'integer function f(integer p) begin ret(f(p)[1$i]); end; integer i;',
     'from i := 0$i to 2$i by 1$i repeat pout(f(i)[1$i]);;', runtime_error='(i)')
case('indirect_recursive_probe_in_loop',
     """integer function outer(integer p)
     integer function inner(integer q) begin ret(outer(q)[1$i]); end;
     begin ret(inner(p)[1$i]); end; integer i;""",
     'from i := 0$i to 2$i by 1$i repeat pout(outer(i)[1$i]);;', runtime_error='(i)')
