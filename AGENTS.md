# Repository Guidelines

## Project Structure & Module Organization

This is a C compiler project built with Bison. Keep parser rules in `src/grammar.y`; Bison generates `y.tab.c` and `y.tab.h` during the build. The lexer and its transition machinery live in `src/lexer.c` and `src/transition_table.c`. Semantic checks and symbol-table integration belong in `src/semantic_actions.c` and `src/symbol_table.c`. Public declarations are in `include/`.

Regression inputs are in `tests/`. Test names describe their purpose: use `lex_...` for lexical coverage, `syn_err_...` for invalid syntax, and `syn_valid_...` for valid programs. Put the expected result first, for example `// EXPECTED: 1 error`, followed by optional `EXPECTED_DIAG`, `EXPECTED_DIAG_LINE`, `EXPECTED_WARNING`, or `EXPECTED_SYMBOL` assertions.

## Build, Test, and Development Commands

Run commands from the repository root:

```bash
./build.sh                    # regenerate Bison output and compile ./compiler
./build.sh tests/syn_valid_combined.txt
./run_tests.sh                # run every test input and validate its assertions
bison -Wall -Werror=conflicts-sr -Werror=conflicts-rr -d -o /tmp/grammar.c src/grammar.y
```

The last command is required after grammar changes: the project must have no shift/reduce or reduce/reduce conflicts. `build.sh` uses `-Wall -Wextra -Werror`, so address every compiler warning.

## Coding Style & Naming Conventions

Use four-space indentation in C and Bison actions. Match the surrounding code: lower-case `snake_case` for functions and variables, upper-case `TOKEN_*` for parser tokens, and explicit `Line %d:` diagnostics. Keep lexer actions small and place parser-specific recovery in `grammar.y`. Report one user-facing diagnostic and increment `global_errors` once for each compiler error; do not print Bison's generic expected-token messages.

## Testing Guidelines

Add a focused test for every lexer, grammar, or recovery change. Verify both the error count and the important diagnostic text. Preserve valid-program coverage when modifying productions, and run the complete suite before submitting changes. Generated files (`compiler`, `y.tab.c`, `y.tab.h`, and `y.output`) are build artifacts and should not be edited manually.

## Commit & Pull Request Guidelines

Recent history uses short imperative subjects, often with a scope or phase, such as `fix: return non-zero exit code on compilation failure` and `Refactor grammar.y: ...`. Follow that style. Keep commits focused. Pull requests should explain the triggering input, the resulting diagnostic or behavior, changed tests, and the full-suite result; link the relevant course requirement when applicable.
