#!/usr/bin/env python3
"""Check inventory coverage and source links; prose still requires human review."""
from collections import Counter
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
DEFINITION = re.compile(
    r'(?m)^(?:(?:static|const|unsigned|signed|long|struct)\s+)*'
    r'(?:[A-Za-z_]\w*)[ \t*\n]+(?:[ \t]*\*+[ \t]*)?'
    r'([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{')
ROW = re.compile(r'^\| \[([A-Za-z_]\w*)\]\(\.\./(src/[^#]+)#L(\d+)\) \| (.+) \| ([^|]+) \|$', re.M)


def definitions(root=ROOT):
    result = {}
    for path in sorted((root/'src').iterdir()):
        if path.suffix not in ('.c', '.y'):
            continue
        source = path.read_text()
        for match in DEFINITION.finditer(source):
            key = (str(path.relative_to(root)), match.group(1))
            assert key not in result, f'Duplicate definition {key}'
            result[key] = source.count('\n', 0, match.start()) + 1
    return result


def main():
    actual = definitions()
    rows = ROW.findall((ROOT/'docs/FUNCTIONS.md').read_text())
    counts = Counter((filename, name) for name, filename, *_ in rows)
    documented = {(filename, name): int(line) for name, filename, line, *_ in rows}
    assert set(actual) == set(documented), dict(missing=sorted(set(actual)-set(documented)), stale=sorted(set(documented)-set(actual)))
    assert all(count == 1 for count in counts.values()), counts
    for key, line in actual.items():
        assert documented[key] == line, f'Outdated source link: {key}'
    print(f'Function inventory: {len(actual)} definitions documented once, with current source links.')


if __name__ == '__main__':
    main()
