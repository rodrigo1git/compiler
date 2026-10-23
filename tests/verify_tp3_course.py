#!/usr/bin/env python3
"""Check TP3 course examples against independent observable expectations."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile
import time
import traceback

import verify_tp3 as oracle
from tp3_course_cases import CASES


def members(text):
    return [] if text == "[]" else text[1:-1].split(", ")


def validate_ir(text):
    sequences, symbols = oracle.parse_output(text)
    assert "main" in sequences, "Missing main sequence"
    assert symbols["course_program:main"]["Use"] == "PROGRAM"
    for name, sequence in sequences.items():
        for index, step in enumerate(sequence):
            if step in ("BF", "BI"):
                assert index != 0 and sequence[index - 1].isdigit(), (name, index, step)
                assert 1 <= int(sequence[index - 1]) <= len(sequence) + 1, (name, index, step)
            if step.startswith(("FRAME ", "CALL ")):
                callee = step.split(" ", 1)[1].split(" ON ")[0]
                assert callee in sequences, f"Call to missing body {callee}"
            if step.startswith("MATERIALIZE "):
                snapshot = symbols[step.split(" ", 1)[1]]
                assert snapshot["Use"] == "OBJECT_VALUE", snapshot
                for binding in members(snapshot.get("Compile-time bindings", "[]")):
                    field, constant = binding.split("=", 1)
                    assert symbols[field].get("Comptime") == "yes", binding
                    assert symbols[constant]["Type"] in ("INTEGER", "SINGLEF"), binding
            for symbol, metadata in symbols.items():
                if metadata.get("Comptime") == "yes":
                    assert step not in (symbol, "ARG " + symbol), f"Residual compile-time operand {step}"
            match = re.fullmatch(r"(?:DEST )?([^\[\]]+)\[(-?\d+)\$i\]", step)
            if match:
                receiver, position = match.groups()
                kind = symbols[receiver]["Declared type"]
                fields = members(symbols[kind]["Effective attributes"])
                position = int(position)
                assert 0 <= position < len(fields), step
                assert symbols[fields[position]].get("Comptime") != "yes", step
    for symbol, metadata in symbols.items():
        if metadata.get("Use") in ("FUNCTION", "METHOD") and metadata.get("Compile-time template") != "yes":
            assert symbol in sequences, f"Declared executable body missing: {symbol}"
        if metadata.get("Use") == "CLASS":
            fields = members(metadata["Effective attributes"])
            assert len(fields) == len(set(fields)), f"Repeated declaration in schema {symbol}"
            assert all(symbols[field]["Use"] == "ATTRIBUTE" for field in fields)
        if metadata.get("Use") in ("VARIABLE", "PARAMETER", "ATTRIBUTE", "FUNCTION", "METHOD"):
            assert "Declared type" in metadata, symbol
        if "Parameters" in metadata:
            assert all(symbols[formal]["Use"] == "PARAMETER" for formal in members(metadata["Parameters"]))
    return sequences, symbols


def verify_result(case, result, analysis):
    expected_error = case.get("error")
    assert result.returncode == (1 if expected_error else 0), result.stderr or result.stdout
    assert not any(marker in result.stdout for marker in ("[LEX]", "[SYNTAX]", "[SCOPE]")), result.stdout
    if expected_error:
        assert re.search(expected_error, result.stderr), result.stderr
        assert "--- RPN ---" not in result.stdout, "Partial RPN was published after an error"
        assert re.search(r"^Line \d+: .+error:", result.stderr, re.MULTILINE), result.stderr
        for diagnostic in case.get("diagnostics", []):
            assert diagnostic in result.stderr, result.stderr
        if "error_at" in case:
            lines = [i for i, text in enumerate(case["source"].splitlines(), 1) if case["error_at"] in text]
            assert len(lines) == 1, lines
            assert re.search(rf"^Line {lines[0]}: .*({expected_error})", result.stderr, re.MULTILINE), result.stderr
        if "error_occurrences" in case:
            assert len(re.findall(expected_error, result.stderr)) == case["error_occurrences"], result.stderr
        return
    assert result.stderr == "", result.stderr
    if analysis:
        assert "--- RPN ---" not in result.stdout
        _, symbols = oracle.parse_output(result.stdout)
    else:
        sequences, symbols = validate_ir(result.stdout)
        flattened = [step for sequence in sequences.values() for step in sequence]
        for step in case.get("contains", []):
            assert step in flattened, (step, flattened)
        for step in case.get("excludes", []):
            assert step not in flattened, (step, flattened)
        for function in case.get("functions", []):
            assert function in sequences, function
        if case.get("execute", True):
            evaluator = oracle.Evaluator(result.stdout)
            evaluator.run()
            assert evaluator.output == case.get("output", []), (evaluator.output, case.get("output", []))
    for symbol, expected in case.get("symbols", {}).items():
        assert symbol in symbols, symbol
        for field, value in expected.items():
            assert symbols[symbol].get(field) == value, (symbol, field, symbols[symbol], value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", nargs="?", default=str(oracle.ROOT / "compiler"))
    parser.add_argument("--case", default="", help="Only run case names containing this text")
    parser.add_argument("--report", type=Path, help="Write a machine-readable result report")
    args = parser.parse_args()
    oracle.BINARY = Path(args.binary).resolve()
    selected = [case for case in CASES if args.case in case["name"]]
    assert selected, "No matching TP3 cases"
    assert len({case["name"] for case in CASES}) == len(CASES), "Duplicate case name"
    records = []
    with tempfile.TemporaryDirectory(prefix="tp3-course-") as directory:
        for case in selected:
            path = Path(directory) / (case["name"] + ".txt")
            path.write_text(case["source"])
            for analysis in (False, True):
                record = {"case": case["name"], "reference": case["reference"],
                          "mode": "analysis" if analysis else "rpn", "passed": False,
                          "expected": "error" if case.get("error") else "success",
                          "evaluated": not analysis and not case.get("error") and case.get("execute", True)}
                if "boundary" in case:
                    record["boundary"] = case["boundary"]
                start = time.monotonic()
                try:
                    result = oracle.compile_file(path, *(["--analyze"] if analysis else []))
                    verify_result(case, result, analysis)
                    record["passed"] = True
                except Exception as error:
                    record["failure"] = str(error)
                    record["traceback"] = traceback.format_exc()
                record["elapsed_seconds"] = round(time.monotonic() - start, 4)
                records.append(record)
    failures = [record for record in records if not record["passed"]]
    if args.report:
        args.report.write_text(json.dumps({
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "compiler_sha256": hashlib.sha256(oracle.BINARY.read_bytes()).hexdigest(),
            "cases": len(selected), "compiler_runs": len(records),
            "passed": len(records) - len(failures), "failed": len(failures), "results": records,
        }, indent=2) + "\n")
    for record in failures:
        print(f"FAIL {record['case']} [{record['mode']}] ({record['reference']}): {record['failure']}")
    print(f"TP3: {len(selected)} scenarios, {len(records) - len(failures)}/{len(records)} compiler runs passed")
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
