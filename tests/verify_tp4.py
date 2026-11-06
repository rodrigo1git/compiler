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
from compiler_cases import load_cases

ROOT = Path(__file__).resolve().parents[1]


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
    cases = load_cases()
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
            textual = 'output_text' in case
            command = ['node', 'runtime/run.mjs'] + ([] if textual else ['--json']) + [str(wasm)]
            executed = run(command)
            actual = executed.stdout.splitlines() if textual else json.loads(executed.stdout)
            expected = case['output_text'] if textual else case.get('output', [])
            assert actual == expected, (case['name'], actual, expected, executed.stderr)
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
                      counts=dict(counts), scenarios=[dict(name=c['name'], expected=c.get('output_text', c.get('output',[])),
                          compile_error=c.get('compile_error'), runtime_error=c.get('runtime_error')) for c in cases],
                      node=run(['node','--version']).stdout.strip(),
                      wabt=json.loads((ROOT/'runtime/package.json').read_text())['dependencies']['wabt'])
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(f'TP4: {sum(counts.values())} scenarios passed: {dict(counts)}')


if __name__ == '__main__': main()
