#!/usr/bin/env python3
"""Check that output switches share semantics and that final Wasm behavior is retained."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from compiler_cases import load_cases
from verify_tp3 import parse_output
from verify_tp4 import verify_compilation

ROOT = Path(__file__).resolve().parents[1]


def compile_source(compiler, source, options):
    return subprocess.run([str(compiler), *options, str(source)], cwd=ROOT,
                          text=True, capture_output=True, timeout=15)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, default=ROOT/'compiler')
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    records = []
    failures = []
    with tempfile.TemporaryDirectory(prefix='compiler-outputs-') as directory:
        folder = Path(directory)
        source, wat = folder/'source.txt', folder/'out.wat'
        for case in load_cases():
            source.write_text(case['source'])
            record = dict(name=case['name'], passed=False)
            try:
                results = []
                for options in ([], ['--rpn'], ['--analyze'],
                                ['--wasm','-o',str(wat)], ['--analyze','--wasm','-o',str(wat)]):
                    wat.unlink(missing_ok=True)
                    result = compile_source(compiler, source, options)
                    verify_compilation(result, case.get('compile_error'))
                    assert wat.exists() == ('--wasm' in options and result.returncode == 0)
                    if result.returncode == 0:
                        assert ('--- RPN ---' in result.stdout) == ('--analyze' not in options)
                    results.append(result)
                assert len({(r.returncode,r.stderr) for r in results}) == 1, 'Output-dependent diagnostics'
                if results[0].returncode == 0:
                    ir = [parse_output(results[i].stdout, include_templates=True)[0] for i in (0,1,3)]
                    assert ir[0] == ir[1] == ir[2], 'Output-dependent intermediate code'
                if args.baseline:
                    final_wat = wat.read_bytes() if wat.exists() else None
                    previous = compile_source(args.baseline.resolve(), source, ['--wasm','-o',str(wat)])
                    verify_compilation(previous, case.get('compile_error'))
                    assert previous.returncode == results[3].returncode, 'Baseline acceptance changed'
                    if previous.returncode == 0:
                        assert final_wat == wat.read_bytes(), 'Generated WAT differs from baseline'
                    # The expected diagnostic is verified above; preserve exact diagnostics as evidence.
                    record['baseline_diagnostics_changed'] = previous.stderr != results[3].stderr
                record['passed'] = True
            except (AssertionError, subprocess.TimeoutExpired) as error:
                record['failure'] = str(error)
                failures.append(record)
                print('FAIL', case['name'], str(error)[:1200], flush=True)
            records.append(record)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(dict(
            compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),
            cases=len(records), compiler_runs=len(records)*(6 if args.baseline else 5),
            passed=len(records)-len(failures), failed=len(failures), results=records), indent=2)+'\n')
    print(f'Output consistency: {len(records)-len(failures)}/{len(records)} scenarios passed')
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
