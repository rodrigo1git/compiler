# Compiler

A C compiler front end for group 24, covering topics **3, 7, 10, 14, 15, 18, 19, 22, 24, 29, 32 and 34**. The lexer uses transition and semantic-action matrices, Bison builds the parser, and a dynamic hash table stores symbols. TP1/TP2 are implemented; the TP3 work currently adds lexical scopes and name resolution.

## Build and test

Requires Bash, GCC and Bison 3.6 or newer. `build.sh` detects Homebrew Bison on macOS. Parser generation rejects all shift/reduce and reduce/reduce conflicts; C compilation uses `-Wall -Wextra -Werror`.

```bash
./build.sh
./compiler tests/valid_scopes.txt
./run_tests.sh
```

The compiler returns 0 only when analysis completes without errors, and 1 otherwise. Warnings do not fail compilation. Diagnostics go to stderr; token traces, scope entries/exits, resolved references, recognized structures and the final symbol table go to stdout.

`run_tests.sh` displays each source file, its compiler output and its exit status. It continues after invalid inputs so that all results can be inspected. It does not compare outputs or decide whether a test passed. To inspect one file, pass it directly to `compiler` after building.

The 38 test files group related cases into complete programs. Case comments identify each scenario, and valid assignments between error cases make continued parsing visible. Inputs that depend on end of file remain separate, including incomplete tokens, unclosed structures and files without a trailing newline. One file preserves CRLF line endings.

Five files cover scopes: `valid_scopes.txt`, `valid_scope_shadowing.txt`, `scope_redeclaration_errors.txt`, `scope_visibility_errors.txt` and `scope_recovery_errors.txt`. They exercise nested declarations, shadowing, declaration order, duplicate names, class/method scopes and restoration after malformed headers.

The original `valid_lexical.txt`, `valid_nested_control.txt` and `valid_topics.txt` describe lexical or syntactic validity. They contain undeclared destinations or repeated global declarations, so scope checking now reports semantic errors for those inputs. They are retained to preserve the original TP1/TP2 coverage.

## Scopes

An active stack starts at `main`. Entering a function, method or class pushes its scope before parameters or members are declared. Leaving it restores the parent. `begin`/`end` statement blocks do not introduce declaration scopes.

Declarations receive qualified names such as `x:main` and `x:main:outer:inner`. Lookup walks from the current scope to its parents and stops at the nearest declaration, which must have the required use. A declaration is visible only after it is recognized. Duplicate declarations in one scope are errors; identical names in different scopes remain independent.

The lexer interns identifier text without declaring it. The parser creates separate scoped declaration entries, so a later lexical occurrence cannot make an earlier use valid. Raw identifier records remain stable while parsing and are omitted from the final scoped symbol table. Constants and chains keep their lexical categories.

Scope guards restore the active stack when Bison discards an incomplete declaration. Rejected duplicate or unnamed declarations use isolated recovery scopes and cannot alter an existing declaration's parameters or members.

This step checks lexical declarations and references, including object receivers and identifier indices. Qualified member selection, inherited member lookup, positional attribute validation, type compatibility, `comptime` evaluation and intermediate-code generation remain separate TP3/TP4 work.

## Language decisions

- Invalid identifiers and out-of-range constants are rejected without registering replacement symbols. `YYerror` starts Bison recovery without a duplicate automatic syntax diagnostic.
- The unsigned magnitude `32768$i` is provisional until the parser sees its sign. Positive use is rejected; negative use registers `-32768$i`. Provisional nodes remain stable while the parser runs and are purged before displaying the final symbol table.
- `=>` and `=<` produce separate `=` and relational-character tokens. The parser reports invalid comparator sequences; the lexer never returns `>=` or `<=` for them.
- Attribute assignments use `obj[index] := expression` or `obj[index] = expression`, through the same `assign` rule. Positional attributes remain valid expression operands. Method calls use `obj.method(arguments)[order]`.
- A single-statement topic 14 loop has two terminators: `from i := 1$i to 2$i by 1$i repeat i := i + 1$i;;`. One terminates its body and the other terminates the loop. A block body uses `end;` after its internal statements.
- Arithmetic grouping parentheses are not supported. Negative numeric constants are supported; general unary negation of variables or expressions is not.
- `CHAIN` identifies a character-chain literal, not a declared `String` type. Newlines inside these literals are omitted from their symbol-table text.

## Source layout

- `src/grammar.y`: valid productions, diagnostic productions and syntax recovery.
- `src/lexer.c`, `src/transition_table.c`, `src/semantic_actions.c`: table-driven tokenization.
- `src/diagnostics.c`: independent diagnostics and pending Bison recovery context.
- `src/scope.c` and `include/scope.h`: scope stack, declarations and lexical name resolution.
- `src/symbol_table.c` and `include/`: stable symbol references and shared declarations.
- `tests/`: source inputs for visual inspection through `run_tests.sh`.

Build outputs `compiler`, `y.tab.c` and `y.tab.h` are generated. Do not edit them manually.

For conflict investigation with Bison 3.7 or newer:

```bash
bison -Wall -Wcounterexamples -Werror=conflicts-sr -Werror=conflicts-rr --report=all -o /tmp/compiler-parser.c src/grammar.y
```
