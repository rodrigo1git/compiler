"""Source-level regressions for receiver identity, signed zero and loop specialization."""
CASES = []

CASES.append(dict(name='correction_loop_comptime_argument',
    source='''review
integer i;
integer function f(comptime integer p)
begin ret(p); end;
begin
from i := 0$i to 3$i by 1$i repeat begin
pout(f(i)[1$i]);
end;
end;
''',
    output=[0, 1, 2]))

CASES.append(dict(name='correction_comptime_signed_zero',
    source='''review
singlef function f(comptime singlef p)
begin ret(p); end;
begin
pout(1.0/f(0.0)[1$i]);
pout(1.0/f(-0.0)[1$i]);
end;
''',
    output_text=['Infinity', '-Infinity']))

CASES.append(dict(name='correction_runtime_signed_zero',
    source='''review
singlef function f(singlef p)
begin ret(p); end;
begin
pout(1.0/f(0.0)[1$i]);
pout(1.0/f(-0.0)[1$i]);
end;
''',
    output_text=['Infinity', '-Infinity']))

CASES.append(dict(name='correction_loop_nested_comptime_argument',
    source='''review
integer i;
integer function outer(integer p)
integer function inner(comptime integer q) begin ret(q); end;
begin
from p := 0$i to 3$i by 1$i repeat begin pout(inner(p)[1$i]); end;
ret(0$i);
end;
begin pout(outer(0$i)[1$i]); end;
''',
    output=[0, 1, 2, 0]))

CASES.append(dict(name='correction_loop_comptime_attribute_index',
    source='''review
class pair code begin integer a; singlef b; end;
pair object;
integer i;
integer function read(comptime integer index) begin pout(object[index]); ret(0$i); end;
begin object[0$i] := 7$i; object[1$i] := 2.5;
from i := 0$i to 2$i by 1$i repeat begin pout(read(i)[1$i]); end;
end;
''',
    output=[7, 0, 2.5, 0]))

CASES.append(dict(name='correction_nested_comptime_receivers',
    source='''review
class inner codeinner begin comptime integer value; end;
class outer codeouter begin
inner child;
integer set(comptime integer p) begin child[0$i] := p; ret(0$i); end;
integer get(integer p) begin ret(child[0$i]); end;
end;
outer a,b;
begin
pout(a.set(3$i)[1$i]);
pout(b.set(7$i)[1$i]);
pout(a.get(0$i)[1$i]);
pout(b.get(0$i)[1$i]);
end;
''',
    output=[0, 0, 3, 7]))

CASES.append(dict(name='correction_nested_comptime_receivers_reversed',
    source='''review
class inner codeinner begin comptime integer value; end;
class outer codeouter begin
inner child;
integer set(comptime integer p) begin child[0$i] := p; ret(0$i); end;
integer get(integer p) begin ret(child[0$i]); end;
end;
outer a,b;
begin
pout(b.set(7$i)[1$i]);
pout(a.set(3$i)[1$i]);
pout(a.get(0$i)[1$i]);
pout(b.get(0$i)[1$i]);
end;
''',
    output=[0, 0, 3, 7]))

CASES.append(dict(name='correction_nested_comptime_same_receiver_fields',
    source='''review
class inner codeinner begin comptime integer value; end;
class outer codeouter begin
inner left,right;
integer setleft(comptime integer p) begin left[0$i] := p; ret(0$i); end;
integer setright(comptime integer p) begin right[0$i] := p; ret(0$i); end;
integer getleft(integer p) begin ret(left[0$i]); end;
integer getright(integer p) begin ret(right[0$i]); end;
end;
outer a;
begin
pout(a.setleft(3$i)[1$i]); pout(a.setright(7$i)[1$i]);
pout(a.getleft(0$i)[1$i]); pout(a.getright(0$i)[1$i]);
end;
''',
    output=[0, 0, 3, 7]))

CASES.append(dict(name='correction_nested_comptime_uninitialized_receiver',
    source='''review
class inner codeinner begin comptime integer value; end;
class outer codeouter begin
inner child;
integer set(comptime integer p) begin child[0$i] := p; ret(0$i); end;
integer get(integer p) begin ret(child[0$i]); end;
end;
outer a,b;
begin pout(a.set(3$i)[1$i]); pout(b.get(0$i)[1$i]); end;
''',
    output=[], compile_error='has no known value'))

CASES.append(dict(name='correction_comptime_loop_control_reference',
    source='''review
comptime integer i;
integer function f(comptime integer p) begin ret(p); end;
begin from i := 0$i to 3$i by 1$i repeat begin pout(f(i)[1$i]); end; end;
''',
    output=[0, 1, 2]))

CASES.append(dict(name='correction_comptime_signed_zero_reversed',
    source='''review
singlef function f(comptime singlef p) begin ret(p); end;
begin pout(1.0/f(-0.0)[1$i]); pout(1.0/f(0.0)[1$i]); end;
''',
    output_text=['-Infinity', 'Infinity']))

CASES.append(dict(name='correction_signed_zero_branch_join',
    source='''review
integer function f(integer p)
singlef x;
begin
if(p>0$i) x := 0.0; else x := -0.0; end_if;
if(1.0/x<0.0) ret(1$i); else ret(2$i); end_if;
end;
begin pout(f(1$i)[1$i]); pout(f(0$i)[1$i]); end;
''',
    output=[2, 1]))


def case(name, declarations, statements, output=(), **options):
    CASES.append(dict(name='correction_' + name,
        source=f'review\n{declarations}\nbegin\n{statements}\nend;\n',
        output=list(output), **options))


INNER = '''class inner codeinner begin comptime integer value;
integer set(comptime integer p) begin value := p; ret(0$i); end;
integer get(integer p) begin ret(value); end; end;'''
OUTER = INNER + '''class outer codeouter begin inner child;
integer set(comptime integer p) begin ret(child.set(p)[1$i]); end;
integer get(integer p) begin ret(child.get(p)[1$i]); end; end;'''
case('nested_method_receivers', OUTER + 'outer a,b;',
     'pout(a.set(3$i)[1$i]); pout(b.set(7$i)[1$i]);'
     'pout(a.get(0$i)[1$i]); pout(b.get(0$i)[1$i]);', [0, 0, 3, 7])
case('deep_method_receivers', OUTER + '''class deep codedeep begin outer child;
integer set(comptime integer p) begin ret(child.set(p)[1$i]); end;
integer get(integer p) begin ret(child.get(p)[1$i]); end; end; deep a,b;''',
     'pout(a.set(3$i)[1$i]); pout(b.set(7$i)[1$i]);'
     'pout(a.get(0$i)[1$i]); pout(b.get(0$i)[1$i]);', [0, 0, 3, 7])
case('nested_inherited_receivers', OUTER + '''class derived codederived begin extends outer;
integer extra; end; derived a,b;''',
     'pout(a.set(3$i)[1$i]); pout(b.set(7$i)[1$i]);'
     'pout(a.get(0$i)[1$i]); pout(b.get(0$i)[1$i]);', [0, 0, 3, 7])
case('nested_copies', OUTER + 'outer a,b;',
     'pout(a.set(3$i)[1$i]); b := a; pout(a.set(7$i)[1$i]);'
     'pout(a.get(0$i)[1$i]); pout(b.get(0$i)[1$i]);', [0, 0, 7, 3])
case('nested_local_activations', OUTER + '''integer function use(comptime integer p)
outer local;
integer function read(integer ignored) begin ret(local.get(0$i)[1$i]); end;
begin pout(local.set(p)[1$i]); ret(read(0$i)[1$i]); end;''',
     'pout(use(3$i)[1$i]); pout(use(7$i)[1$i]); pout(use(3$i)[1$i]);', [0, 3, 0, 7, 0, 3])
case('nested_argument_copy', OUTER + '''outer a;
integer function use(outer local) begin pout(local.set(7$i)[1$i]); ret(local.get(0$i)[1$i]); end;''',
     'pout(a.set(3$i)[1$i]); pout(use(a)[1$i]); pout(a.get(0$i)[1$i]);', [0, 0, 7, 3])

for index, error in [(0, None), (1, 'return type: SINGLEF')]:
    case('nested_discarded_' + str(index), '''class pair codepair begin integer a; singlef b; end;
class inner codeinner begin comptime integer index; pair data;
integer read(integer p) begin ret(0$i); ret(data[index]); end; end;
class outer codeouter begin inner child;
integer set(comptime integer p) begin child[0$i] := p; ret(0$i); end;
integer get(integer p) begin ret(child.read(p)[1$i]); end; end; outer a,b;''',
         f'pout(a.set(0$i)[1$i]); pout(b.set({index}$i)[1$i]); '
         'pout(a.get(0$i)[1$i]); pout(b.get(0$i)[1$i]);',
         [0, 0, 0, 0] if error is None else [], compile_error=error)

case('signed_zero_language_equality', 'integer unused;',
     'if(0.0==-0.0) pout(1$i); else pout(2$i); end_if; pout(toi(-0.0));', [1, 0])

FUNCTION = 'integer function f(comptime integer p) begin ret(p); end;'
case('loop_descending', 'integer i; ' + FUNCTION,
     'from i := 2$i to -1$i by 1$i repeat begin pout(f(i)[1$i]); end;', [2, 1, 0])
case('loop_empty', 'integer i; ' + FUNCTION,
     'from i := 3$i to 3$i by 1$i repeat begin pout(f(i)[1$i]); end; pout(i);', [3])
case('loops_nested_arguments', 'integer i,j; ' + FUNCTION,
     'from i := 0$i to 2$i by 1$i repeat begin '
     'from j := 0$i to 2$i by 1$i repeat begin pout(f(i+j)[1$i]); end; end;', [0, 1, 1, 2])
case('loop_control_mutation', 'integer i; ' + FUNCTION,
     'from i := 0$i to 4$i by 1$i repeat begin pout(f(i)[1$i]); i := i+1$i; end; pout(i);', [0, 2, 4])
case('loop_effect_count', 'integer i; integer function f(comptime integer p) begin pout(p); ret(p); end;',
     'from i := 0$i to 3$i by 1$i repeat begin pout(f(i)[1$i]); end;', [0, 0, 1, 1, 2, 2])
case('loop_argument_return', 'integer i; ' + FUNCTION,
     'from i := 0$i to 3$i by 1$i repeat begin pout(f(f(i)[1$i])[1$i]); end;', [0, 1, 2])
case('loop_runtime_unknown_control', FUNCTION + '''integer function use(integer bound)
integer i; begin if(bound>0$i) from i := 0$i to 2$i by 1$i repeat begin pout(f(1$i)[1$i]); end; end_if; ret(0$i); end;''',
     'pout(use(2$i)[1$i]);', [1, 1, 0])
case('loop_unknown_argument', FUNCTION + '''integer function use(integer bound)
integer i; begin from i := 0$i to 2$i by 1$i repeat begin pout(f(bound)[1$i]); end; ret(0$i); end;''',
     'pout(use(2$i)[1$i]);', compile_error='is not known at compile time')
case('loop_runtime_error_preserved', 'integer i; ' + FUNCTION,
     'from i := 0$i to 3$i by 1$i repeat begin pout(f(i)[1$i]); pout(200$i*200$i); end;',
     [0], runtime_error='(d)')
case('loop_assigned_invariant', 'integer i,j; ' + FUNCTION,
     'from i := 0$i to 3$i by 1$i repeat begin j := 7$i; pout(f(j)[1$i]); end;', [7, 7, 7])
case('loop_argument_order', '''integer i,j;
integer function f(comptime integer p, comptime integer q) begin ret(p+q); end;''',
     'from i := 0$i to 3$i by 1$i repeat begin pout(f(i,j=7$i)[2$i,1$i]); end;', [7, 8, 9])
case('loop_argument_order_invariant', '''integer i,j;
integer function f(comptime integer p, comptime integer q) begin ret(p+q); end;''',
     'from i := 0$i to 1$i by 0$i repeat begin pout(f(j,j=7$i)[2$i,1$i]); end;', execute=False)
case('loop_branch_requirement', 'integer i; ' + FUNCTION,
     'from i := 0$i to 3$i by 1$i repeat begin '
     'if(i==0$i) pout(f(7$i)[1$i]); else pout(f(i)[1$i]); end_if; end;', [7, 1, 2])
case('loop_nonterminating_invariant', 'integer i; ' + FUNCTION,
     'from i := 0$i to 1$i by 0$i repeat begin pout(f(f(1$i)[1$i])[1$i]); end;', execute=False)
case('loop_nonterminating_inner_finite', 'integer i,j; ' + FUNCTION,
     'from i := 0$i to 1$i by 0$i repeat begin '
     'from j := 0$i to 2$i by 1$i repeat begin pout(f(j)[1$i]); end; end;', execute=False)

case('discarded_loop_argument_valid', FUNCTION + '''integer function use(integer ignored)
integer i; begin ret(0$i);
from i := 0$i to 3$i by 1$i repeat begin pout(f(i)[1$i]); end; end;''', 'pout(use(0$i)[1$i]);', [0])
case('discarded_loop_argument_bad_type', '''class pair code begin integer a; singlef b; end; pair object;
integer function read(comptime integer index) begin ret(object[index]); end;
integer function use(integer ignored) integer i; begin ret(0$i);
from i := 0$i to 2$i by 1$i repeat begin pout(read(i)[1$i]); end; end;''',
     'pout(use(0$i)[1$i]);', compile_error='return type: SINGLEF')
