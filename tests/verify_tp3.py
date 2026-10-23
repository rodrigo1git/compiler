#!/usr/bin/env python3
"""Regression checks from TP2/TP3 and the course's LR -> PI examples.

The small evaluator verifies observable effects of well-typed test programs. It
is a test oracle for the IR contract, not a substitute for TP4 type checking.
"""
import copy
import json
import operator
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "compiler"


def compile_file(path, *options):
    result = subprocess.run([str(BINARY), *options, str(path)], text=True,
                            capture_output=True, timeout=10)
    assert "AddressSanitizer" not in result.stderr, result.stderr
    assert "LeakSanitizer" not in result.stderr, result.stderr
    assert "runtime error:" not in result.stderr, result.stderr
    assert result.returncode in (0, 1), result.stderr
    return result


def parse_output(text):
    sequences, symbols, sequence = {}, {}, None
    for line in text.splitlines():
        if line == "--- RPN ---":
            sequence = sequences.setdefault("main", [])
        elif line.startswith("--- FUNCTION "):
            sequence = sequences.setdefault(line[len("--- FUNCTION "):-4], [])
        elif line == "--- SYMBOL TABLE ---":
            sequence = None
        elif line.startswith("Lexeme: "):
            fields = dict(part.split(": ", 1) for part in line.split(" | "))
            previous = symbols.get(fields["Lexeme"], {})
            rank = lambda entry: 2 if "Use" in entry else int(entry.get("Type") in ("INTEGER", "SINGLEF"))
            if rank(fields) >= rank(previous):
                symbols[fields["Lexeme"]] = fields
        elif sequence is not None and re.match(r"^\d+ ", line):
            address, step = line.split(" ", 1)
            assert int(address) == len(sequence) + 1
            sequence.append(step)
    return sequences, symbols


class Evaluator:
    """Eager operand values, pending call frames, lexical parents and receivers."""
    def __init__(self, text):
        self.code, self.symbols = parse_output(text)
        self.global_frame = {"scope": "main", "values": {}, "parent": None, "receiver": None}
        self.output = []
        self.fuel = 100000

    def frame_for(self, symbol, frame):
        scope = symbol.split(":", 1)[1]
        while frame is not None:
            if frame["scope"] == scope:
                return frame
            frame = frame["parent"]
        raise AssertionError(f"Missing lexical activation for {symbol}")

    def location(self, symbol, frame):
        if self.symbols[symbol].get("Use") == "ATTRIBUTE":
            assert frame["receiver"] is not None, symbol
            return frame["receiver"], symbol
        return self.frame_for(symbol, frame)["values"], symbol

    def copy_object(self, value, kind):
        source_fields = self.symbols[value["__class__"]]["Effective attributes"][1:-1].split(", ")
        target_fields = self.symbols[kind]["Effective attributes"][1:-1].split(", ")
        result = {"__class__": kind}
        for source, target in zip(source_fields, target_fields):
            if not target or source not in value or self.symbols[target].get("Comptime") == "yes":
                continue
            field = value[source]
            result[target] = self.copy_object(field, self.symbols[target]["Declared type"]) \
                if isinstance(field, dict) else copy.deepcopy(field)
        return result

    def read(self, symbol, frame):
        values, key = self.location(symbol, frame)
        if key not in values:
            kind = self.symbols[symbol].get("Declared type")
            assert kind in self.symbols and self.symbols[kind].get("Use") == "CLASS", symbol
            values[key] = {"__class__": kind}  # Scalar fields must be assigned before reading.
        return values[key]

    def attribute(self, text, frame):
        match = re.fullmatch(r"(.+)\[(.+)\]", text)
        assert match, text
        object_name, index_name = match.groups()
        index = int(index_name[:-2]) if index_name.endswith("$i") else self.read(index_name, frame)
        kind = self.symbols[object_name]["Declared type"]
        attributes = self.symbols[kind].get("Effective attributes", self.symbols[kind]["Attributes"])[1:-1].split(", ")
        return self.read(object_name, frame), attributes[index]

    def run(self, name="main", frame=None):
        frame = frame or self.global_frame
        code, stack, pending, pc = self.code[name], [], [], 0
        binary = {"+": operator.add, "-": operator.sub, "*": operator.mul,
                  "/": operator.truediv, "<": operator.lt, ">": operator.gt,
                  "<=": operator.le, ">=": operator.ge, "==": operator.eq, "!=": operator.ne}
        while pc < len(code):
            self.fuel -= 1
            assert self.fuel > 0, "Non-terminating generated IR"
            step = code[pc]
            pc += 1
            if step.startswith("FRAME "):
                target, *receiver = step[6:].split(" ON ")
                declaration = self.symbols[target].get("Source function", target)
                scope = declaration.split(":", 1)[1]
                parent = frame
                while parent is not None and parent["scope"] != scope:
                    parent = parent["parent"]
                is_method = self.symbols[target]["Use"] == "METHOD"
                if parent is None and is_method:
                    enclosing_scope = scope.rsplit(":", 1)[0]
                    parent = frame
                    while parent is not None and parent["scope"] != enclosing_scope:
                        parent = parent["parent"]
                active_receiver = self.read(receiver[0], frame) if receiver else frame["receiver"]
                assert parent is not None or is_method, target
                pending.append({"scope": scope + ":" + declaration.split(":", 1)[0],
                                "values": {}, "parent": parent or self.global_frame,
                                "receiver": active_receiver})
            elif step.startswith("ARG "):
                stack.append((pending[-1]["values"], step[4:]))
            elif step.startswith("CALL "):
                target = step[5:].split(" ON ")[0]
                stack.append(self.run(target, pending.pop()))
            elif step == ":=":
                values, key = stack.pop()
                value = stack.pop()
                kind = self.symbols[key].get("Declared type")
                if isinstance(value, dict) and kind in self.symbols:
                    values[key] = self.copy_object(value, kind)
                else:
                    values[key] = copy.deepcopy(value)
            elif step.startswith("MATERIALIZE "):
                metadata = self.symbols[step.split(" ", 1)[1]]
                value = copy.deepcopy(stack.pop())
                for binding in metadata.get("Compile-time bindings", "[]")[1:-1].split(", "):
                    if not binding:
                        continue
                    field, constant = binding.split("=", 1)
                    value[field] = int(constant[:-2]) if constant.endswith("$i") else \
                        struct.unpack("f", struct.pack("f", float(constant.replace("s", "e"))))[0]
                stack.append(value)
            elif step in binary:
                right, left = stack.pop(), stack.pop()
                result = binary[step](left, right)
                if step == "/" and isinstance(left, int) and isinstance(right, int):
                    result = int(result)
                if isinstance(result, float):
                    result = struct.unpack("f", struct.pack("f", result))[0]
                stack.append(result)
            elif step == "DUP":
                stack.append(stack[-1])
            elif step == "DROP":
                stack.pop()
            elif step == "TOI":
                stack.append(int(stack.pop()))
            elif step == "POUT":
                self.output.append(stack.pop())
            elif step == "RET":
                result = stack.pop()
                assert not stack and not pending
                return result
            elif step == "BI":
                pc = stack.pop() - 1
            elif step == "BF":
                target, condition = stack.pop(), stack.pop()
                if not condition:
                    pc = target - 1
            elif step.isdigit():
                stack.append(int(step))
            elif step.startswith('"'):
                stack.append(json.loads(step))
            elif step.endswith("$i"):
                stack.append(int(step[:-2]))
            elif step.startswith("DEST "):
                stack.append(self.attribute(step[5:], frame))
            elif "[" in step:
                values, key = self.attribute(step, frame)
                stack.append(values[key])
            elif step in self.symbols and self.symbols[step]["Type"] == "SINGLEF":
                stack.append(struct.unpack("f", struct.pack("f", float(step.replace("s", "e"))))[0])
            elif pc < len(code) and code[pc] == ":=":
                stack.append(self.location(step, frame))
            else:
                stack.append(self.read(step, frame))
        assert not stack and not pending


def main():
    checks = 0
    # Every public input has an explicit expected acceptance status in analysis mode.
    valid = {"valid_comment_at_eof.txt", "valid_crlf.txt", "valid_eof_without_newline.txt",
             "valid_scopes.txt", "valid_scope_shadowing.txt", "rpn_valid.txt",
             "tp3_calls.txt", "tp3_methods.txt", "tp3_control.txt"}
    for path in sorted((ROOT / "tests").glob("*.txt")):
        result = compile_file(path, "--analyze")
        assert result.returncode == (0 if path.name in valid else 1), (path.name, result.stderr)
        assert "[LEX]" not in result.stdout and "[SYNTAX]" not in result.stdout
        checks += 1
        result = compile_file(path)
        assert result.returncode == (0 if path.name in valid else 1), (path.name, result.stderr)
        if result.returncode:
            assert "--- RPN ---" not in result.stdout
        checks += 1

    for name, expected in {
        "tp3_calls.txt": [10, 8, 19, 7, 7, 8, 10, 7, 9, 0, 2.5, 0.0],
        "tp3_methods.txt": [10, 12, 21, 20, 10, 31, 6, 99, 20, 20, 7],
        "tp3_control.txt": [15, 24, 24, 3, "BF"],
    }.items():
        result = compile_file(ROOT / "tests" / name)
        assert result.returncode == 0, result.stderr
        assert "[SCOPE]" not in result.stdout
        evaluator = Evaluator(result.stdout)
        evaluator.run()
        assert evaluator.output == expected, (name, evaluator.output, expected)
        checks += 1
    result = compile_file(ROOT / "tests/rpn_valid.txt")
    evaluator = Evaluator(result.stdout)
    evaluator.run()
    for symbol, expected in {"arithmetic_result": 10, "difference": 12, "quotient": 3,
                             "minimum": -32768, "side_effect": 7, "self_result": 3}.items():
        assert evaluator.global_frame["values"][symbol + ":main"] == expected
    checks += 1

    header = "review\ninteger result;\ninteger function f(integer p, integer q)\nbegin ret(p+q); end;\n"
    cases = [
        (header + "begin result := f(1$i)[1$i,2$i]; end;", "has 1 arguments; expected 2"),
        (header + "begin result := f(1$i,2$i)[1$i]; end;", "has 1 entries; expected 2"),
        (header + "begin result := f(1$i,2$i)[1$i,1$i]; end;", "repeats position 1"),
        (header + "begin result := f(1$i,2$i)[0$i,2$i]; end;", "outside 1..2"),
        (header + "begin result := f(1$i,2$i)[1.0,2$i]; end;", "must be INTEGER"),
        ("review\ninteger x;\nbegin x[0$i] := 1$i; end;", "is not an object"),
        ("review\nclass c code begin integer x; end;\nc object;\n"
         "begin object[1$i] := 1$i; end;", "does not exist"),
        ("review\ninteger result;\nclass c code begin integer x; end;\nc object;\n"
         "begin result := object.missing(1$i)[1$i]; end;", "does not exist"),
        ("review\ninteger result;\nclass c code begin integer x; end;\nc object;\n"
         "begin result := object.x(1$i)[1$i]; end;", "is not a method"),
        ("review\ncomptime integer x;\nbegin x := 1$i; end;", None),
        ("review\ninteger result;\ninteger function f(comptime integer x)\n"
         "begin ret(x); end;\nbegin result := f(1$i)[1$i]; end;", None),
        ("review\nclass a code begin integer x; end;\n"
         "class b code begin integer x; end;\ninteger x;\nbegin x := 1$i; end;",
         None),
        ("review\nclass a ca begin integer x; end;\n"
         "class b cb begin integer x; end;\na x; b y;\nbegin x := y; end;",
         "requires matching class codes"),
        ("review\nclass a ca begin integer x; end;\n"
         "class b cb begin extends a; integer y; end;\nb object;\nbegin object[0$i] := 1$i; end;",
         None),
    ]
    method = "integer read(integer p) begin ret(p); end;"
    cases.append(("review\ninteger result;\nclass a ca begin " + method + " end;\n"
                  "class b cb begin " + method + " end;\n"
                  "class c cc begin extends a,b; integer x; end;\nc object;\n"
                  "begin result := object.read(1$i)[1$i]; end;", "is ambiguous"))
    with tempfile.TemporaryDirectory(prefix="tp3-check-") as directory:
        path = Path(directory) / "input.txt"
        for source, diagnostic in cases:
            path.write_text(source)
            result = compile_file(path)
            if diagnostic is None:
                assert result.returncode == 0, result.stderr
            else:
                assert result.returncode == 1 and diagnostic in result.stderr, (diagnostic, result.stderr)
                assert "--- RPN ---" not in result.stdout
                assert re.search(r"Line \d+:", result.stderr), result.stderr
            checks += 1
        # TP3 p.4 / Generación slide 66: no numeric compatibility pass in LR -> PI.
        path.write_text("review\ninteger x;\nbegin x := 1.5 + 1$i; end;")
        assert compile_file(path).returncode == 0
        checks += 1
    trace = compile_file(ROOT / "tests/tp3_control.txt", "--analyze", "--trace")
    assert trace.returncode == 0 and "[LEX]" in trace.stdout
    checks += 1
    print(f"PASS: {checks} compiler checks (including evaluated course examples)")


if __name__ == "__main__":
    main()
