# Compiler

A C compiler front end for group 24, covering topics **3, 7, 10, 14, 15, 18, 19, 22, 24, 29, 32 and 34**. The lexer uses transition and semantic-action matrices, Bison builds the parser, and a dynamic hash table stores symbols. TP3 includes lexical scopes, ordered declaration metadata, calls with explicit argument order, methods, inherited positional attributes, structural class compatibility and compile-time evaluation. The implemented rules and their course references are documented in [docs/tp3_implementation.md](docs/tp3_implementation.md).

The current delivery report is [docs/tp3_report.md](docs/tp3_report.md).
Displaying symbolic bodies for uninvoked compile-time templates remains pending;
those bodies are retained internally and marked in the symbol table. The existing
test suite has not been extended for the latest object-capture corrections.

## Build and test

Requires Bash, GCC and Bison 3.6 or newer; automated verification also requires Python 3. `build.sh` detects Homebrew Bison on macOS. Parser generation rejects all shift/reduce and reduce/reduce conflicts; C compilation uses `-Wall -Wextra -Werror`.

```bash
./build.sh
./compiler tests/valid_scopes.txt
./run_tests.sh
```

The compiler returns 0 only when the selected stage completes without errors, and 1 otherwise. Warnings do not fail compilation. The default TP3 output contains numbered RPN and the symbol table, with all diagnostics on stderr. `--rpn` is an explicit alias for this mode. `--analyze` performs the same semantic processing and compile-time evaluation while suppressing RPN output. `--trace` enables debugging traces independently of the selected stage. Without it, token, syntax and scope traces are suppressed as required by TP3 p.12.

`run_tests.sh` builds and runs both `tests/verify_tp3.py` and `tests/verify_tp3_course.py`, returning nonzero if an expectation fails. The suite includes 107 original checks plus 170 course scenarios compiled in both analysis and generation modes (340 additional compiler runs). The course suite executes 103 valid programs through the IR evaluator, checks 60 rejection scenarios, and keeps seven compile-only cases separate. Five of those seven explicitly verify the TP4 boundary; the other two are intentionally nonterminating runtime programs. `./run_tests.sh --show --analyze --trace` displays the inputs and compiler output for manual inspection; this display mode does not grade results. To inspect one file, pass it directly to `compiler` after building.

The 43 public source files group related cases into complete programs. The course scenarios are defined with source references and observable expectations in `tests/tp3_course_cases.py`; each is materialized as a separate temporary input so one error cannot hide another scenario. Case comments identify each scenario, and valid assignments between error cases make continued parsing visible. Inputs that depend on end of file remain separate, including incomplete tokens, unclosed structures and files without a trailing newline. One file preserves CRLF line endings.

Five files cover scopes: `valid_scopes.txt`, `valid_scope_shadowing.txt`, `scope_redeclaration_errors.txt`, `scope_visibility_errors.txt` and `scope_recovery_errors.txt`. They exercise nested declarations, shadowing, declaration order, duplicate names, class/method scopes and restoration after malformed headers.

The original `valid_lexical.txt`, `valid_nested_control.txt` and `valid_topics.txt` describe lexical or syntactic validity. They contain undeclared destinations or repeated global declarations, so scope checking now reports semantic errors for those inputs. They are retained to preserve the original TP1/TP2 coverage.

## Scopes

An active stack starts at `main`. Entering a function, method or class pushes its scope before parameters or members are declared. Leaving it restores the parent. `begin`/`end` statement blocks do not introduce declaration scopes.

Declarations receive qualified names such as `x:main` and `x:main:outer:inner`. Lookup walks from the current scope to its parents and stops at the nearest declaration, which must have the required use. A declaration is visible only after it is recognized. Duplicate declarations in one scope are errors; identical names in different scopes remain independent.

The lexer interns identifier text without declaring it. The parser creates separate scoped declaration entries, so a later lexical occurrence cannot make an earlier use valid. Raw identifier records remain stable while parsing and are omitted from the final scoped symbol table. Constants and chains keep their lexical categories.

Scope guards restore the active stack when Bison discards an incomplete declaration. Function guards also restore the enclosing RPN sequence. Rejected duplicate or unnamed declarations use isolated recovery scopes and cannot alter an existing declaration's parameters or members.

Scope checking covers lexical declarations, object receivers, identifier indices and method selection. Methods are searched in the object's class first and then its bases; distinct inherited matches are errors (topic 32). Inheritance cycles are errors. Repeated paths to the same declaration are deduplicated. Effective attributes contain inherited attributes first, traversing bases in source order, followed by own declarations. Known integer indices are checked for existence and visibility. Classes sharing a code must have the same effective attribute types in the same order; attribute names and methods do not participate in this comparison. Numeric runtime type compatibility belongs to TP4 for direct RPN generation.

## Reverse Polish Notation (RPN)

`include/rpn.h` and `src/rpn.c` provide a dynamic sequence of symbol references, operators and jump targets. Steps use one-based addresses. Reserved targets must be completed before printing; freeing a sequence preserves its borrowed symbol-table entries. Separate sequences can represent separate function bodies.

The parser generates RPN for arithmetic, assignments, selections, FROM loops, POUT, explicit TOI conversions, calls, functions and methods with RET, and positional attributes including inheritance:

```bash
./build.sh
./compiler --rpn tests/rpn_valid.txt
./run_tests.sh --rpn
```

Supported operations are `+`, `-`, `*`, `/`, negative numeric constants, ordinary `:=` assignments and the topic 18 assignments inside expressions. The existing grammar determines precedence and left associativity. Embedded assignments use `DUP` to retain the assigned value before the ordinary destination and `:=` steps consume a copy. Destinations are distinguished from value operands internally, although both display their symbol names. Steps retain their source lines for later diagnostics.

Expression fragments and complete function bodies are retained during parsing, with resolved symbol references and control boundaries. A subsequent specialization pass evaluates compile-time state and rebuilds numbered branches before output. The output is printed only if the whole input is valid. Argument fragments are retained until the explicit evaluation order is known.

Conditions support `<`, `>`, `<=`, `>=`, `==` and `!=`. An `IF` emits its condition followed by a reserved destination and `BF`. An `ELSE` adds a reserved destination and `BI` to skip its body after the true branch. The parser stack retains each pending address, including nested selections; destinations are completed at `ELSE` and `end_if`. A destination can point to the first step after the program. Both single statements and `begin`/`end` blocks are supported.

Topic 14 `FROM` loops initialize their control variable before the first comparison. When the initial constant is less than or equal to the final constant, the comparison is `<` and the update adds the BY constant. Otherwise, the comparison is `>` and the update subtracts it. Equal bounds execute no iterations. After the body, the update reads the control variable's current value and `BI` returns to the comparison; `BF` exits the loop. Each loop context stays on the parser stack, so loops and selections can be nested. Loop generation adds no sign restriction on BY; type checks, including the required integer control variable and constants, remain deferred to TP4 for RPN.

An explicit `toi(expression)` appends `TOI` after its operand fragment and leaves a value for the enclosing expression. A `pout(expression)` or `pout(chain)` appends `POUT`, which consumes its argument. For example, `pout(toi(3.9));` produces `3.9 TOI POUT`. These operations remain part of the intermediate representation; this mode does not execute the program.

CHAIN operands display with double quotes and escaped backslashes, quotes and control characters, keeping every step on one numbered line. This distinguishes literals such as `"BF"`, `"3$i"` and `"a:main"` from operators and other operands. Their symbol-table entries and source-language lexical rules are unchanged.

Each executable function or method instance has an independent sequence, displayed under `--- FUNCTION name:scope ---` after the main sequence; its symbol-table usage distinguishes functions and methods. Step addresses and branch destinations are local to each body. Entering a nested declaration selects its own sequence; closing or discarding it restores the enclosing one. The symbol table retains return types and an ordered list of scoped formal parameters. Class entries retain their code, own and effective attributes, methods and bases. `comptime` declarations and formal parameters retain an explicit marker. Function entries also show final local compile-time bindings for their instances. Context-dependent source bodies are marked as compile-time templates; concrete instances are emitted when invoked, with generated `$ctN` labels linked to their source function.

An explicit `RET` consumes the expression value and exits the current function. A default epilogue returns `0$i` for INTEGER functions or `0.0` for SINGLEF functions when execution reaches the end of the body. Earlier RET steps exit before that epilogue.

Calls validate argument count, evaluation-order length and an INTEGER permutation of positions 1..N. `FRAME function` prepares a separate pending activation, followed by each argument fragment, `ARG formal`, and ordinary `:=` copying in the requested order. `CALL function` consumes that pending activation and yields its return value. Reordering evaluation never changes the argument-to-formal association. Method frames additionally retain `ON object`; unqualified method calls inherit the active receiver. These are abstract IR operations, with no machine layout or assembler convention imposed. See the detailed contract and course references in the implementation document.

Positional operands display as `object[index]`, and destinations as `DEST object[index]`. They retain object and index symbol references; implicit attribute operands inside methods use the current receiver. Dynamic index type and runtime bounds checks remain for TP4; accesses to compile-time attributes require a determinable position. Separate instances keep separate attribute storage. `MATERIALIZE @valueN` overlays the captured compile-time fields onto an object value at its evaluation point, before later argument effects. The snapshot is retained in TS metadata; object stores copy only runtime fields, while compile-time fields are bound by the specialization pass.

Snapshots also retain nested object fields through `Object fields` references to
other OBJECT_VALUE entries. Compile-time state is identified by its root object
and declaration path, so sibling fields, copied objects and function activations
remain independent. Whole-object reads participate in specialization dependencies;
cached code is reused only for matching compile-time bindings.

`rpn_valid.txt` covers arithmetic, signed constants, assignment values, chains and side effects. `rpn_errors.txt` covers missing operands, out-of-range constants and undeclared destinations, including the suppression of partial output. Existing lexer and parser tests retain the other invalid-expression cases.

Compile-time variables record known values and their uses are replaced with constant symbol references. Known branches process only their selected path for compile-time effects; unknown branches reject compile-time modifications at the assignment, including modifications through calls. FROM loops with compile-time updates are expanded while their control remains known; ordinary loops retain branches. Function templates with compile-time parameters or captured compile-time state are specialized for their bindings, with separate activation environments. Runtime effects such as POUT remain in the resulting code. `DROP` consumes a runtime value when its surrounding compile-time assignment is removed but its effects must remain.

The evaluator uses INTEGER and SINGLEF values and explicit TOI. Values stored in compile-time declarations must be evaluable as their declared type; this is required to materialize those constants, and does not add a general runtime numeric compatibility pass. No zero is assumed for an uninitialized compile-time variable. Compile-time expansion has a 100,000-iteration budget and specialization has a depth limit of 128; exceeding them reports an evaluation resource diagnostic, not a new restriction on BY or runtime recursion.

Functions and methods may return INTEGER or SINGLEF. Object returns are rejected as a language restriction. Direct object copies require matching class codes for distinct classes and use corresponding effective attribute positions. Different attribute names remain compatible when the types and order agree. General runtime numeric compatibility and conversion range checks remain deferred to TP4, as required by TP3 p.4 and Generación slide 66.

Simple Reduction is not applicable: neither group member belongs to the 2011 study plan. Full page-by-page test coverage, example adaptations and corrected defects are recorded in [docs/tp3_validation.md](docs/tp3_validation.md).

`tp3_calls.txt`, `tp3_methods.txt` and `tp3_control.txt` cover the added generation. The verifier evaluates the topic 19 examples (10 and 8), nested and recursive calls, lexical parents, copy semantics, independent objects, inherited method lookup, default returns, nested branches, loops and explicit conversion. Additional generated inputs check required diagnostics and the boundary of the implemented stage.

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
- `src/rpn.c` and `src/codegen.c`: dynamic intermediate representation and retained body generation.
- `src/specialize.c`: compile-time environments, control processing, specialization and final branch reconstruction.
- `src/symbol_table.c` and `include/`: stable symbol references and shared declarations.
- `tests/`: public source inputs, the original verifier/IR evaluator, course scenario definitions and the course verifier.

Build outputs `compiler`, `y.tab.c` and `y.tab.h` are generated. Do not edit them manually.

For conflict investigation with Bison 3.7 or newer:

```bash
bison -Wall -Wcounterexamples -Werror=conflicts-sr -Werror=conflicts-rr --report=all -o /tmp/compiler-parser.c src/grammar.y
```

## Course validation

```bash
./run_tests.sh
python3 tests/verify_tp3_course.py --case comptime_loop
python3 tests/verify_tp3_course.py --report docs/tp3_test_results.json
```

The report records the compiler hash, each scenario's course reference and mode,
observable evaluation status, and pass/fail result. Numeric compatibility examples
reserved for TP4 are not reported as successfully checked numeric programs.
