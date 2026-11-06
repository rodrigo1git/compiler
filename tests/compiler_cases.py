"""One set of source expectations for all compiler outputs."""
from pathlib import Path
import re
from tp3_course_cases import CASES as COURSE
from tp4_cases import CASES
from correction_cases import CASES as CORRECTIONS

ROOT = Path(__file__).resolve().parents[1]


def load_cases():
    cases = [dict(case, compile_error=re.escape(case['compile_error']) if case.get('compile_error') else None)
             for case in CASES + CORRECTIONS]
    cases += [dict(case, compile_error=case.get('error')) for case in COURSE]
    for filename, expected in [('tp3_templates.txt',[3,4,7,8,6]),
                               ('tp3_comptime_copies.txt',[10,16,11,16,3,7,14]),
                               ('tp3_comptime_specialization.txt',[3,7,3]),
                               ('tp3_methods.txt',[10,12,21,20,10,31,6,99,20,20,7]),
                               ('rpn_valid.txt',[])]:
        cases.append(dict(name=filename, source=(ROOT/'tests'/filename).read_text(), output=expected))
    cases.append(dict(name='tp3_calls.txt', source=(ROOT/'tests/tp3_calls.txt').read_text(),
                      output=[10,8,19,7,7,8], runtime_error='(i)'))
    cases.append(dict(name='tp3_control.txt', source=(ROOT/'tests/tp3_control.txt').read_text(),
                      output=[15,24,24], runtime_error='(g)'))
    return cases
