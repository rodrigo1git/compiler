# Compiler

A C compiler front end for the assigned TP1 lexical topics and selected TP2 grammar topics. The lexer uses a transition table, Bison builds the parser, and the symbol table stores identifiers, constants, and strings.

## Build and test

Run from the repository root:

```bash
./build.sh
./compiler tests/syn_valid_combined.txt
./run_tests.sh
bison -Wall -Werror=conflicts-sr -Werror=conflicts-rr -o /tmp/parser.c src/grammar.y
```

`build.sh` regenerates the Bison parser and compiles with `-Wall -Wextra -Werror`. `run_tests.sh` builds first and checks every fixture in `tests/`; run it separately from another build because both replace the generated executable.

## Source layout

- `src/grammar.y`: grammar and syntax-error recovery.
- `src/lexer.c`, `src/transition_table.c`, `src/semantic_actions.c`: tokenization and lexical actions.
- `src/symbol_table.c` and `include/`: symbol table and shared declarations.
- `tests/`: one-purpose lexical and syntactic regression fixtures with `EXPECTED` assertions.
- `grammar_changes_report.md`: current implementation notes, validation, and known specification gaps.
- `TP1_TP2_REPORT_ADDENDUM.md`: corrections to outdated descriptions in the submitted PDF report.

See [AGENTS.md](AGENTS.md) for contributor conventions. Build outputs such as `compiler`, `y.tab.c`, and `y.tab.h` are generated and should not be edited manually.
