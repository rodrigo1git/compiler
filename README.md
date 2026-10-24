# Compiler

A university project developed at Universidad Nacional del Centro de la Provincia de Buenos Aires (UNICEN), in Tandil, Argentina.

An educational compiler front end written in C for a small imperative language. It demonstrates lexical analysis, parsing, scope checking, compile-time evaluation and intermediate code generation in Reverse Polish Notation (RPN).

## Requirements

- Bash
- GCC
- GNU Bison 3.6 or newer

## Build

Clone the repository and build the compiler:

```bash
git clone https://github.com/rodrigo1git/compiler.git
cd compiler
./build.sh
```

## Usage

Compile a bundled example to intermediate code:

```bash
./compiler tests/rpn_valid.txt
```

The default output contains numbered RPN instructions and the symbol table. To analyze a file without printing RPN:

```bash
./compiler --analyze tests/rpn_valid.txt
```

Add `--trace` to either command to display debugging traces. Source diagnostics include line numbers and are written to standard error. The compiler returns `0` when the input is accepted and `1` when an error occurs.

## Example

Save this program as `example.txt`:

```text
example
integer result;
begin
    result := 2$i + 3$i * 4$i;
    pout(result);
end;
```

Generate its intermediate representation:

```bash
./compiler example.txt
```

The arithmetic expression produces the RPN sequence `2$i 3$i 4$i * +`. The output also includes instructions to assign the result and print it. The compiler generates this representation without executing the program.

## Test inputs

Display the bundled inputs and their compiler results:

```bash
./run_tests.sh
```

The script rebuilds the compiler and shows each input, its output and its exit status for manual inspection. It continues through inputs containing intentional errors, which are expected to return `1`. Use `./run_tests.sh --analyze` to inspect analysis results without RPN.

## Project status

The language supports arithmetic, conditionals, loops, functions, methods, inheritance and compile-time variables. This is a work in progress; numeric type compatibility checks and native code generation are still under development.
