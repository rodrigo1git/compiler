#!/usr/bin/env python3
"""Compile to WAT, validate actual Wasm and execute independent expectations."""
import argparse
from collections import Counter
import json
import hashlib
import re
from datetime import datetime, timezone
from pathlib import Path
import subprocess
import tempfile
from tp3_course_cases import CASES as COURSE
from tp4_cases import CASES

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_ERRORS = {
    'specialization_resource_guard': ('(i)', []),
    'nested_and_recursive_calls': ('(i)', [6]),
    'comptime_recursive_parameters': ('(i)', []),
    'explicit_conversion_example': ('(g)', []),
    'explicit_conversion_operand': ('(g)', []),
    'comptime_runtime_recursion': ('(i)', []),
    'ordinary_recursion_does_not_specialize': ('(i)', []),
}
COMPILE_ERRORS = {
    'comptime_division_by_zero': 'Integer division by zero required by comptime assignment',
    'comptime_float_and_conversion': 'fractional information',
    'mixed_operands_deferred': 'Incompatible operand',
    'float_assignment_deferred': 'Incompatible assignment',
    'loop_types_deferred': 'FROM control',
    'parameter_types_deferred': 'Incompatible assignment',
    'return_types_deferred': 'return type',
}


def run(command, **options):
    return subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=15, **options)


def verify_compilation(result, expected_error=None):
    sanitizer = r'(?:Address|Leak|UndefinedBehavior|Memory|Thread)Sanitizer(?::| has encountered)'
    undefined_behavior = r':\d+(?::\d+)?: runtime error:'
    assert not re.search(sanitizer + '|' + undefined_behavior, result.stderr), result.stderr
    assert result.returncode == (1 if expected_error else 0), result.stderr
    if expected_error:
        assert isinstance(expected_error, str), 'Expected a diagnostic pattern'
        assert re.search(expected_error, result.stderr), (expected_error, result.stderr)
        assert '--- RPN ---' not in result.stdout, 'Partial RPN after a compilation error'
    else:
        assert not result.stderr, result.stderr


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--report', type=Path)
    parser.add_argument('--compiler', type=Path, default=ROOT / 'compiler')
    parser.add_argument('--case', default='', help='Only run case names containing this text')
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    cases = [dict(case, compile_error=re.escape(case['compile_error']) if case.get('compile_error') else None)
             for case in CASES]
    for original in COURSE:
        case = dict(original)
        if original.get('error'):
            if case['name'] in RUNTIME_ERRORS:
                case['compile_error'] = None
            elif case['name'] == 'comptime_integer_overflow':
                case.update(output=[], compile_error=None)
            elif case['name'] == 'comptime_attribute_dynamic_position':
                case.update(output=[3], compile_error=None)
            else:
                error = COMPILE_ERRORS.get(case['name'])
                case['compile_error'] = re.escape(error) if error else original['error']
                cases.append(case)
                continue
        error = COMPILE_ERRORS.get(case['name'])
        case['compile_error'] = re.escape(error) if error else None
        if case['name'] in RUNTIME_ERRORS:
            case['runtime_error'], case['output'] = RUNTIME_ERRORS[case['name']]
        cases.append(case)
    for filename, expected in [('tp3_templates.txt',[3,4,7,8,6]),
                               ('tp3_comptime_copies.txt',[10,16,11,16,3,7,14]),
                               ('tp3_comptime_specialization.txt',[3,7,3])]:
        cases.append(dict(name=filename, source=(ROOT/'tests'/filename).read_text(), output=expected))
    cases = [case for case in cases if args.case in case['name']]
    assert cases, f'No cases matching {args.case!r}'
    counts = Counter()
    with tempfile.TemporaryDirectory(prefix='compiler-tp4-') as directory:
        folder = Path(directory)
        for case in cases:
            source, wat, wasm = (folder / ('program' + suffix) for suffix in ('.txt','.wat','.wasm'))
            for path in (wat,wasm): path.unlink(missing_ok=True)
            source.write_text(case['source'])
            compiled = run([str(compiler),'--wasm','-o',str(wat),str(source)])
            expected_error = case.get('compile_error')
            try:
                verify_compilation(compiled, expected_error)
            except AssertionError as error:
                raise AssertionError((case['name'], str(error))) from error
            if expected_error:
                assert not wat.exists(), case['name']
                counts['compile_errors'] += 1
                continue
            if case['name'] in ('integer_limits', 'operand_snapshot'):
                assert '| Use: AUXILIARY' in compiled.stdout, case['name']
            assembled = run(['node','runtime/assemble.mjs',str(wat),str(wasm)])
            assert assembled.returncode == 0, (case['name'],assembled.stderr)
            if not case.get('execute',True) and not case.get('runtime_error'):
                try:
                    subprocess.run(['node','runtime/run.mjs',str(wasm)],cwd=ROOT,stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL,timeout=0.5)
                except subprocess.TimeoutExpired:
                    counts['nonterminating'] += 1
                    continue
                raise AssertionError((case['name'],'Expected nonterminating loop'))
            executed = run(['node','runtime/run.mjs','--json',str(wasm)])
            actual = json.loads(executed.stdout)
            assert actual == case.get('output',[]), (case['name'],actual,case.get('output'),executed.stderr)
            runtime_error = case.get('runtime_error')
            if runtime_error:
                assert executed.returncode == 1 and runtime_error in executed.stderr, (case['name'],executed.stderr)
                counts['runtime_errors'] += 1
            else:
                assert executed.returncode == 0 and not executed.stderr, (case['name'],executed.stderr)
                counts['normal'] += 1
        source.write_text('review\ninteger x;\nbegin x := 1$i; end;')
        before = source.read_bytes()
        result = run([str(compiler),'--wasm','-o',str(source),str(source)])
        verify_compilation(result, 'Output path must differ from the source file')
        assert source.read_bytes() == before
        result = run([str(compiler),'--wasm','-o',str(folder/'missing'/'out.wat'),str(source)])
        verify_compilation(result, 'Cannot write')
    if args.report:
        paths = list((ROOT/'src').glob('*')) + list((ROOT/'include').glob('*'))
        paths += [ROOT/'build.sh'] + list((ROOT/'runtime').glob('*.mjs'))
        paths += list((ROOT/'tests').glob('*.py')) + [ROOT/'tests/run_all.sh']
        report = dict(date=datetime.now(timezone.utc).isoformat(),
                      compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),
                      sources={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths if p.is_file()},
                      counts=dict(counts), scenarios=[dict(name=c['name'], expected=c.get('output',[]),
                          compile_error=c.get('compile_error'), runtime_error=c.get('runtime_error')) for c in cases],
                      node=run(['node','--version']).stdout.strip(),
                      wabt=json.loads((ROOT/'runtime/package.json').read_text())['dependencies']['wabt'])
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(f'TP4: {sum(counts.values())} scenarios passed: {dict(counts)}')


if __name__ == '__main__': main()
