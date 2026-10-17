# Compiler

A C compiler front end for group 24, covering topics **3, 7, 10, 14, 15, 18, 19, 22, 24, 29, 32 and 34**. The lexer uses transition and semantic-action matrices, Bison builds the parser, and a dynamic hash table stores symbols. TP1/TP2 are implemented; the TP3 work currently adds lexical scopes, name resolution and partial RPN generation.

## Build and test

Requires Bash, GCC and Bison 3.6 or newer. `build.sh` detects Homebrew Bison on macOS. Parser generation rejects all shift/reduce and reduce/reduce conflicts; C compilation uses `-Wall -Wextra -Werror`.

```bash
./build.sh
./compiler tests/valid_scopes.txt
./run_tests.sh
```

The compiler returns 0 only when analysis completes without errors, and 1 otherwise. Warnings do not fail compilation. Diagnostics go to stderr; token traces, scope entries/exits, resolved references, recognized structures and the final symbol table go to stdout.

`run_tests.sh` displays each source file, its compiler output and its exit status. It continues after invalid inputs so that all results can be inspected. It does not compare outputs or decide whether a test passed. To inspect one file, pass it directly to `compiler` after building.

The 40 test files group related cases into complete programs. Case comments identify each scenario, and valid assignments between error cases make continued parsing visible. Inputs that depend on end of file remain separate, including incomplete tokens, unclosed structures and files without a trailing newline. One file preserves CRLF line endings.

Five files cover scopes: `valid_scopes.txt`, `valid_scope_shadowing.txt`, `scope_redeclaration_errors.txt`, `scope_visibility_errors.txt` and `scope_recovery_errors.txt`. They exercise nested declarations, shadowing, declaration order, duplicate names, class/method scopes and restoration after malformed headers.

The original `valid_lexical.txt`, `valid_nested_control.txt` and `valid_topics.txt` describe lexical or syntactic validity. They contain undeclared destinations or repeated global declarations, so scope checking now reports semantic errors for those inputs. They are retained to preserve the original TP1/TP2 coverage.

## Scopes

An active stack starts at `main`. Entering a function, method or class pushes its scope before parameters or members are declared. Leaving it restores the parent. `begin`/`end` statement blocks do not introduce declaration scopes.

Declarations receive qualified names such as `x:main` and `x:main:outer:inner`. Lookup walks from the current scope to its parents and stops at the nearest declaration, which must have the required use. A declaration is visible only after it is recognized. Duplicate declarations in one scope are errors; identical names in different scopes remain independent.

The lexer interns identifier text without declaring it. The parser creates separate scoped declaration entries, so a later lexical occurrence cannot make an earlier use valid. Raw identifier records remain stable while parsing and are omitted from the final scoped symbol table. Constants and chains keep their lexical categories.

Scope guards restore the active stack when Bison discards an incomplete declaration. Rejected duplicate or unnamed declarations use isolated recovery scopes and cannot alter an existing declaration's parameters or members.

Scope checking covers lexical declarations and references, including object receivers and identifier indices. Qualified member selection, inherited member lookup, positional attribute validation, type compatibility and `comptime` evaluation remain separate TP3/TP4 work.

## Reverse Polish Notation (RPN): expressions, assignments and control flow

`include/rpn.h` and `src/rpn.c` provide a dynamic sequence of symbol references, operators and jump targets. Steps use one-based addresses. Reserved targets must be completed before printing; freeing a sequence preserves its borrowed symbol-table entries. Separate sequences can represent separate function bodies.

The parser can generate RPN for scalar arithmetic, assignments, selections and FROM loops in a separate mode:

```bash
./build.sh
./compiler --rpn tests/rpn_valid.txt
./run_tests.sh --rpn
```

Supported operations are `+`, `-`, `*`, `/`, negative numeric constants, ordinary `:=` assignments and the topic 18 assignments inside expressions. The existing grammar determines precedence and left associativity. Embedded assignments use `DUP` to retain the assigned value before the ordinary destination and `:=` steps consume a copy. Destinations are distinguished from value operands internally, although both display their symbol names. Steps retain their source lines for later diagnostics.

Expression fragments are combined during reductions and emitted when an assignment is recognized. The output is printed only if the whole input is valid. RPN mode suppresses token, syntax and scope traces; the default mode retains its previous output.

Conditions support `<`, `>`, `<=`, `>=`, `==` and `!=`. An `IF` emits its condition followed by a reserved destination and `BF`. An `ELSE` adds a reserved destination and `BI` to skip its body after the true branch. The parser stack retains each pending address, including nested selections; destinations are completed at `ELSE` and `end_if`. A destination can point to the first step after the program. Both single statements and `begin`/`end` blocks are supported.

Topic 14 `FROM` loops initialize their control variable before the first comparison. When the initial constant is less than or equal to the final constant, the comparison is `<` and the update adds the BY constant. Otherwise, the comparison is `>` and the update subtracts it. Equal bounds execute no iterations. After the body, the update reads the control variable's current value and `BI` returns to the comparison; `BF` exits the loop. Each loop context stays on the parser stack, so loops and selections can be nested. Loop generation adds no sign restriction on BY; type checks, including the required integer control variable and constants, remain deferred to TP4 for RPN.

`rpn_valid.txt` covers arithmetic, signed constants, assignment values, chains and side effects. `rpn_errors.txt` covers missing operands, out-of-range constants and undeclared destinations, including the suppression of partial output. Existing lexer and parser tests retain the other invalid-expression cases.

Functions, calls, POUT, conversions, attributes, classes and `comptime` declarations are rejected explicitly in RPN mode until their generation stages are implemented. They remain available to the existing lexer/parser/scope analysis in default mode. General type compatibility checks remain deferred to TP4, as required for Reverse Polish Notation (RPN).

`run_tests.sh` accepts `--rpn` to display the same inputs, outputs and return codes in this mode. It still does not compare expected answers or produce PASS/FAIL results. Existing inputs using features from later stages will report the corresponding unsupported-feature diagnostics in this mode.

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
- `src/rpn.c` and `src/codegen.c`: dynamic intermediate representation and expression generation.
- `src/symbol_table.c` and `include/`: stable symbol references and shared declarations.
- `tests/`: source inputs for visual inspection through `run_tests.sh`.

Build outputs `compiler`, `y.tab.c` and `y.tab.h` are generated. Do not edit them manually.

For conflict investigation with Bison 3.7 or newer:

```bash
bison -Wall -Wcounterexamples -Werror=conflicts-sr -Werror=conflicts-rr --report=all -o /tmp/compiler-parser.c src/grammar.y
```
