# TP3 delivery report

This is the current group 24 implementation report. `report.md` and `report.pdf`
remain the historical TP1/TP2 submission. The course references below refer to
`Tp03-2026.pdf`, available in `/home/rodrigo/facultad/compi`.

## Scope and course basis

The compiler generates dynamic, numbered reverse Polish notation and records
types, uses and scoped declaration references in one symbol table (pp.1-3).
Independent executable bodies, ordered parameters, copy semantics and default
primitive returns implement the common function and method requirements (p.5).
FROM, embedded assignments, explicit argument order, compile-time variables,
structural class codes, positional attributes and inherited member lookup follow
the assigned topics 14, 18, 19, 22, 24, 29, 32 and 34 (pp.4-11).

The user resolved details left open by the course: reject compile-time writes
under unknown control; compare class structures by attribute types and order;
place inherited attributes first; identify diamond members by their declaration;
require every argument ordinal once; and prohibit object returns. Neither member
belongs to the 2011 plan, so Simple Reduction is excluded.

## Current corrections

The topic 22 rule requires the value recorded at each use, rather than a value
captured by an earlier invocation. Whole-object reads now participate in
compile-time dependency classification, including class-valued attributes and
nested compile-time leaves. Dependency and effect information propagates through
calls. Cached instances are reused only when their compile-time environments
match, preserving runtime calls and outputs.

Object copy semantics (p.5), the explicit evaluation order (p.7), compile-time
substitution (p.8) and structural copies (p.9) apply together. Captures therefore
include nested objects and occur when each argument is evaluated and copied.
Internal identities combine the root object and the declaration path to a field.
Copied objects, sibling fields of the same class and separate activations retain
independent state. Ordinary parameters do not become compile-time formals merely
because their argument happened to be known at a call site.

MATERIALIZE references an immutable OBJECT_VALUE symbol-table entry. Immediate
compile-time leaves appear in `Compile-time bindings`; nested snapshots appear
in `Object fields` as field-to-child references. Runtime value copies omit
compile-time destination storage, which is bound during specialization. Type
traversals guard their active path against cycles. A recursive value that cannot
be captured finitely receives an evaluation diagnostic at its use.

The source location required for diagnostics (p.12) remains available after loop
expansion. Repeated visits to one operation with the same diagnostic are counted
once. The identity includes body, operation, line and message, with object paths
included for nested-field errors. Different operations on the same line and
different sibling fields remain separate errors.

## Validation and limits of the evidence

The existing suite contains 107 checks plus 170 scenarios compiled in two modes,
for 447 compiler checks per build. The verifiers and their cases are unchanged by
this correction. They pass against the corrected normal and instrumented builds;
the instrumented build uses ASan, UBSan and leak detection. Bison reports zero
conflicts, and C warnings are treated as errors.

Additional complete programs are created only in `/tmp`. Their checks include:

- Captured values 3, 7 and 3, including a wrapper and an object argument.
- An object parameter containing a nested compile-time field, producing 3.
- Two nesting levels, sibling fields of the same type, independent copies and
  separate function activations.
- Both explicit argument orders around a call that changes the source object.
- Implicit nested attributes in methods with independent receivers.
- Compatible copies with mixed compile-time and ordinary field qualifiers.
- Diagnostic deduplication across expanded iterations, distinct operations on
  one line, distinct nested paths, unknown control and recursive type traversal.

These checks do not extend permanent suite coverage. The existing evaluator is
unchanged and does not independently interpret the new nested `Object fields`
metadata; numeric output checks are supplemented by inspection of the snapshot
graph and its references. The older hashed JSON reports remain historical records
of their stated binaries, rather than reports for this correction.

## Pending template presentation

The current compiler retains raw bodies of context-dependent functions and
methods, marks them as templates in the symbol table, and emits concrete
specializations when called. An uninvoked template's body is not displayed. This
is an outstanding presentation gap against the per-declaration body and visible
IR requirements on pp.1,5,12; this correction does not close it.

The pending decision is how to expose a body whose compile-time formal values
are unavailable, while keeping executable IR free of unresolved compile-time
operands. A separate symbolic-template section would preserve source parameters
and show the retained numbered body, with concrete instances in the executable
section. The course requires clear visualization and per-function code, but does
not prescribe that exact section layout or template format.

One further display choice is whether to show the source template for every
context-dependent declaration, or only when no concrete instance exists. Showing
every source template plus its concrete instances gives a stable one-to-one
account of declarations and makes specialization easier to inspect. No values
should be invented to force otherwise unavailable specializations. Call targets
must continue to reference concrete executable bodies. Template validation must
remain distinct from checks that reject residual compile-time operands in
executable code. No template display or test adjustment is implemented here.

## TP4 boundary

Assembler generation, general runtime numeric compatibility and the associated
conversion checks remain TP4 for direct RPN generation, as stated on p.4.
Compile-time evaluation still requires known, representable values of the
appropriate type. Compile-time loop and specialization resource limits protect
the compiler; they do not restrict ordinary runtime loops or recursion.

Implementation details and the existing coverage map remain in
[tp3_implementation.md](tp3_implementation.md) and
[tp3_validation.md](tp3_validation.md).
