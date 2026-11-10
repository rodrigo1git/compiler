#!/usr/bin/env python3
"""Exercise memory growth, architectural boundaries, deep classes and full diagnostics."""
import argparse
import json
from pathlib import Path
import resource
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(command, **options):
    return subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=60, **options)


def checked(command, **options):
    result = run(command, **options)
    assert result.returncode == 0, (command, result.stdout[-2000:], result.stderr[-2000:])
    return result


def binary_classes(depth):
    declarations = ['class c0 code0 begin integer value; end;']
    for i in range(1, depth + 1):
        declarations.append(f'class c{i} code{i} begin c{i-1} left,right; end;')
    return '\n'.join(declarations)


def compile_program(compiler, folder, text, name):
    source, wat, wasm = [folder / (name + suffix) for suffix in ('.txt', '.wat', '.wasm')]
    source.write_text(text)
    checked([str(compiler), '--analyze', '--wasm', '-o', str(wat), str(source)])
    checked(['node', 'runtime/assemble.mjs', str(wat), str(wasm)])
    return source, wat, wasm


def native_checks(folder, sanitize):
    native = folder / 'storage-test'
    flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g'] if sanitize else []
    checked(['gcc', '-Wall', '-Wextra', '-Werror', '-std=gnu99', '-Iinclude', *flags,
             'tests/test_storage.c', 'src/classes.c', 'src/layout.c', 'src/scope.c',
             'src/diagnostics.c', 'src/memory.c', 'src/symbol_table.c', 'src/comptime_objects.c',
             'src/comptime_values.c', 'src/numeric.c', '-lm', '-o', str(native)])
    def small_stack():
        resource.setrlimit(resource.RLIMIT_STACK, (256 * 1024, resource.getrlimit(resource.RLIMIT_STACK)[1]))
    checked([str(native), 'deep'], preexec_fn=None if sanitize else small_stack)
    checked([str(native), 'alignment'])
    result = checked([str(native), 'diagnostics'])
    context = 'x' * 4989 + 'CONTEXT_END'
    assert result.stderr == (f'Line 11: Syntax error: Recovered message ({context})\n'
                             'Line 12: Syntax error: flushed context\n'
                             'Line 13: Syntax error: Invalid syntax\n'), result.stderr
    result = checked([str(native), 'layout'])
    assert result.stderr.count('not representable in wasm32') == 3, result.stderr
    for mode in ('overflow', 'multiply'):
        result = run([str(native), mode])
        assert result.returncode == 1 and result.stderr == 'Compiler allocation size overflow\n', result.stderr


def wasm_checks(compiler, folder):
    # Each c20 occupies 12*2^20-4 bytes: both main and the called frame exceed old caps.
    large = ('review\n' + binary_classes(20) + '''
c20 first,second; integer keep;
integer function use(integer p)
c20 local; integer previous;
begin pout(previous); previous := p; ret(previous); end;
begin
keep := 7$i; pout("before growth");
pout(use(3$i)[1$i]); pout(use(4$i)[1$i]);
pout(keep); pout("after growth");
end;
''')
    _, large_wat, wasm = compile_program(compiler, folder, large, 'large')
    host = (ROOT / 'runtime/host.mjs').as_uri()
    code = f'''import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {{execute}} from {json.dumps(host)};
const values = [];
const instance = await execute(await fs.readFile({json.dumps(str(wasm))}), x => values.push(x));
assert.deepEqual(values, ['before growth', 0, 3, 0, 4, 7, 'after growth']);
assert.ok(instance.exports.memory.buffer.byteLength > 16 * 1024 * 1024);
const size = instance.exports.memory.buffer.byteLength;
instance.exports.main();
assert.deepEqual(values.slice(7), values.slice(0, 7));
assert.equal(instance.exports.memory.buffer.byteLength, size);
'''
    checked(['node', '--input-type=module', '-e', code])

    # A runtime allocation failure must reach the real host with a descriptive source line.
    limited = folder / 'limited.wat'
    limited.write_text(large_wat.read_text().replace('(memory (export "memory") 1)',
                                                  '(memory (export "memory") 1 2)'))
    limited_wasm = folder / 'limited.wasm'
    checked(['node', 'runtime/assemble.mjs', str(limited), str(limited_wasm)])
    failed = run(['node', 'runtime/run.mjs', '--json', str(limited_wasm)])
    assert failed.returncode == 1 and json.loads(failed.stdout) == [], failed
    assert 'Line 1: Runtime error: Unable to allocate linear memory' in failed.stderr, failed.stderr

    # Initialization must handle deep composition with a small engine call stack.
    depth = 1600
    declarations = ['class d0 tag0 begin integer value; end;']
    declarations += [f'class d{i} tag{i} begin d{i-1} child; end;' for i in range(1, depth)]
    deep = 'review\n' + '\n'.join(declarations) + f'\nd{depth-1} object;\nbegin pout(1$i); end;'
    _, _, wasm = compile_program(compiler, folder, deep, 'deep')
    result = checked(['node', '--stack-size=128', 'runtime/run.mjs', '--json', str(wasm)])
    assert json.loads(result.stdout) == [1]

    # Architectural rejection must terminate with a diagnostic and leave no partial WAT.
    source, output = folder / 'oversized.txt', folder / 'oversized.wat'
    source.write_text('review\n' + binary_classes(29) + '\nbegin pout(1$i); end;\n')
    result = run([str(compiler), '--wasm', '-o', str(output), str(source)])
    assert result.returncode == 1 and 'not representable in wasm32' in result.stderr, result.stderr
    assert not output.exists()

    _, wat, _ = compile_program(compiler, folder, 'review\ninteger x;\nbegin x := 1$i; end;', 'small')
    text = wat.read_text()
    assert '(memory (export "memory") 1)' in text
    # Instrument only this test module: expose allocator state and force deterministic grow failure.
    text = text.replace('(memory (export "memory") 1)', '(memory (export "memory") 1 2)')
    text = text.replace('(func $allocate ', '(func $allocate (export "testAllocate") ', 1)
    text = text.replace('(global $sp ', '(global $sp (export "testSp") ', 1)
    text = text.replace('(memory (export', '(global $grows (export "grows") (mut i32) (i32.const 0))\n(memory (export', 1)
    text = text.replace('memory.grow', 'global.get $grows\ni32.const 1\ni32.add\nglobal.set $grows\nmemory.grow')
    wat.write_text(text)
    wasm = folder / 'allocator.wasm'
    checked(['node', 'runtime/assemble.mjs', str(wat), str(wasm)])
    code = f'''import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
const env = {{printInteger() {{}}, printFloat() {{}}, printString() {{}},
    runtimeFail(code, line) {{ throw new Error(`${{code}}:${{line}}`); }} }};
const {{instance}} = await WebAssembly.instantiate(await fs.readFile({json.dumps(str(wasm))}), {{env}});
const {{memory, testAllocate: allocate, testSp: sp, grows}} = instance.exports;
assert.equal(memory.buffer.byteLength, 65536);
const initial = sp.value;
const first = allocate(Number(65536n - initial), 10);
assert.equal(sp.value, 65536n);
assert.equal(grows.value, 0);
new Uint8Array(memory.buffer)[first] = 123;
const next = allocate(4, 11);
assert.equal(next, 65536);
assert.equal(memory.buffer.byteLength, 131072);
assert.equal(new Uint8Array(memory.buffer)[first], 123);
assert.deepEqual([...new Uint8Array(memory.buffer, next, 4)], [0,0,0,0]);
sp.value = BigInt(next);
new Uint8Array(memory.buffer).fill(255, next, next + 4);
allocate(4, 12);
assert.deepEqual([...new Uint8Array(memory.buffer, next, 4)], [0,0,0,0]);
const before = sp.value;
assert.throws(() => allocate(65536, 13), /8:13/);
assert.equal(sp.value, before);
// Exact 4 GiB end reaches grow; exceeding it fails before grow, without wrapping.
sp.value = 0xfffffff0n;
const attempts = grows.value;
assert.throws(() => allocate(16, 14), /8:14/);
assert.equal(grows.value, attempts + 1);
assert.throws(() => allocate(32, 15), /8:15/);
assert.equal(grows.value, attempts + 1);
assert.equal(sp.value, 0xfffffff0n);
// An i32 size above 2 GiB must be treated as unsigned and attempt growth.
sp.value = initial;
assert.throws(() => allocate(-2147483648, 16), /8:16/);
assert.equal(grows.value, attempts + 2);
'''
    checked(['node', '--input-type=module', '-e', code])

    # Active data segments larger than one page must fit at instantiation time.
    chain = 'z' * 70000
    _, _, wasm = compile_program(compiler, folder, f'review\ninteger x;\nbegin pout("{chain}"); end;', 'strings')
    result = checked(['node', 'runtime/run.mjs', '--json', str(wasm)])
    assert json.loads(result.stdout) == [chain]

    invalid = folder / 'syntax.txt'
    invalid.write_text('review\nbegin end;')
    result = run([str(compiler), '--analyze', str(invalid)])
    assert result.returncode == 1, result
    assert "expected identifier, 'integer', 'singlef', 'class' or 'comptime'" in result.stderr, result.stderr
    assert 'another valid token' not in result.stderr


def static_data_boundaries(compiler, folder):
    # Explicit expectations cover empty data, 16-byte alignment and page boundaries.
    cases = [(16, 16, 1, 1), (17, 32, 1, 1), (31, 32, 1, 1), (32, 32, 1, 1),
             (65535, 65536, 1, 2), (65536, 65536, 1, 2), (65537, 65552, 2, 2)]
    programs = []
    for end, base, initial_pages, final_pages in cases:
        chain = 'z' * (end - 17) if end > 16 else None
        output = f'pout("{chain}"); pout(x); pout("{chain}");' if chain is not None else 'pout(x);'
        source = f'review\ninteger x;\nbegin x := 123$i; {output} end;'
        _, _, wasm = compile_program(compiler, folder, source, f'boundary{end}')
        programs.append(dict(path=str(wasm), end=end, base=base, initial=initial_pages, final=final_pages))
    host = (ROOT / 'runtime/host.mjs').as_uri()
    code = f'''import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {{execute}} from {json.dumps(host)};
const env = {{printInteger() {{}}, printFloat() {{}}, printString() {{}},
    runtimeFail(code, line) {{ throw new Error(`${{code}}:${{line}}`); }} }};
for (const test of {json.dumps(programs)}) {{
    const bytes = await fs.readFile(test.path);
    const {{instance}} = await WebAssembly.instantiate(bytes, {{env}});
    const memory = instance.exports.memory;
    assert.equal(memory.buffer.byteLength, test.initial * 65536, test.path);
    const initial = new Uint8Array(memory.buffer);
    if (test.end > 16) {{
        assert.ok(initial.subarray(16, test.end - 1).every(x => x === 122));
        assert.equal(initial[test.end - 1], 0);
    }}
    const saved = initial.slice(0, test.base);
    for (let run = 0; run < 2; run++) {{
        instance.exports.main();
        assert.equal(memory.buffer.byteLength, test.final * 65536, test.path);
        assert.deepEqual(new Uint8Array(memory.buffer, 0, test.base), saved, test.path);
        assert.equal(new DataView(memory.buffer).getInt32(test.base + 8, true), 123, test.path);
    }}
    const output = [];
    await execute(bytes, value => output.push(value));
    const text = 'z'.repeat(Math.max(0, test.end - 17));
    assert.deepEqual(output, test.end > 16 ? [text, 123, text] : [123], test.path);
}}
'''
    checked(['node', '--input-type=module', '-e', code])

    # The frame fits in i32, but adding static data can exceed the address space.
    # Sum of c0,c2,...,c28 is 2^32-64 bytes; the frame header adds eight bytes.
    declarations = binary_classes(28) + '\n' + '\n'.join(f'c{i} object{i};' for i in range(0, 29, 2))
    compile_program(compiler, folder, f'review\n{declarations}\nbegin pout(1$i); end;', 'near_top')
    source, wat = folder / 'global_overflow.txt', folder / 'global_overflow.wat'
    source.write_text(f'review\n{declarations}\nbegin pout("' + 'z' * 32 + '"); end;')
    result = run([str(compiler), '--wasm', '-o', str(wat), str(source)])
    assert result.returncode == 1, result
    assert 'Global storage is not representable in wasm32' in result.stderr, result.stderr
    assert not wat.exists()


def host_pointer_check():
    # Simulate a high address at the JS boundary without allocating gigabytes.
    host = (ROOT / 'runtime/host.mjs').as_uri()
    code = f'''import assert from 'node:assert/strict';
import {{execute}} from {json.dumps(host)};
const RealUint8Array = Uint8Array;
globalThis.Uint8Array = class {{
    constructor() {{ return {{length: 0x80000002, [0x80000000]: 65, [0x80000001]: 0,
        subarray(start, end) {{ assert.equal(start, 0x80000000); assert.equal(end, start + 1);
            return new RealUint8Array([65]); }} }}; }}
}};
WebAssembly.instantiate = async (_, {{env}}) => ({{instance: {{exports: {{memory: {{buffer: null}},
    main() {{ env.printString(-2147483648); }} }} }} }});
const output = [];
await execute(null, x => output.push(x));
assert.deepEqual(output, ['A']);
'''
    checked(['node', '--input-type=module', '-e', code])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=ROOT / 'compiler')
    parser.add_argument('--sanitize', action='store_true', help='Use ASan/UBSan for the native harness')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='compiler-storage-') as directory:
        folder = Path(directory)
        native_checks(folder, args.sanitize)
        wasm_checks(args.compiler.resolve(), folder)
        static_data_boundaries(args.compiler.resolve(), folder)
        host_pointer_check()
    print('Storage: deep classes, full diagnostics, >16 MiB programs, growth, reuse and wasm32 boundaries passed.')


if __name__ == '__main__':
    main()
