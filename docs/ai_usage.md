# AI assistance record

This record distinguishes the original report's attribution from the work visible in the current Codex conversation. It does not reconstruct missing conversations.

## Original implementation

The supplied report attributes these parts to AI assistance:

- Gemini: semantic-action matrix, action descriptions and test cases.
- Claude: conversion of the proposed grammar into Yacc/Bison and help with conflicts.
- An unspecified AI tool: initial dynamic hash-table implementation.

Only the semantic-action prompt is transcribed in the supplied report:

> Dada esta matriz de transición de estados quiero que hagas la matriz de acciones semánticas. Hay un total de 15 estados. con transiciones llamadas cada una "AS" y su número correspondiente. cada flecha de cambio de estado muestran los símbolos con los cuales tienen que aparecer en la cadena para pasar al siguiente. El símbolo C es cualquiera, D es dígito, L es una letra (minús y mayús). estamos haciendo un traductor léxico y ahora mismo estamos reconociendo los tokens. La tercera imagen es un ejemplo. El segundo es el grafo real. fijate como se especifica cada AS aparte. La primera imagen es la consigna. pensar mucho y no cometas errores. Respira profundo y pensa paso por paso.

The original screenshots and any unrecorded prompts should be retained by the team if they have them. There is no available complete prompt history for the other original interventions; no substitute prompts are presented as historical facts.

## Current correction work

Tool: Codex, OpenAI. The assistant compared the original project, compiler2 and the submitted ZIP, executed their fixtures, checked the official Bison manual and course PDFs, and proposed the correction plan. The user approved preserving both attribute assignment operators and the two terminators of a single-statement loop.

Available implementation instructions, quoted from this conversation:

> quiero que hagas un plan de como podrias mejorar el archivo del zip para que cumpla con lo que nos critico el profesor (el profesor no recibio el informe por error asi que tene en cuenta eso) ademas quiero que lo que propongas mantenga el mismo formato limpio y en ingles como esta ahora mismo todo el codigo. solo plan.

> quiero que hagas un commit con el mensaje correcto, revisa otros comentarios de commits para seguir el patron

> bueno. entonces mira el plan de antes que me diste para poder solucionar lo que me dijo el profe e implementa la solucion para que quede todo. antes de empezar. tenes alguna duda en particular? hay algo en lo que no estas seguro?

> no. implementa todo. acepto tus dos propuestas

The planning request also required examining Bison documentation and the course's theory directory, justifying the current behavior and proposed solutions before listing steps, and covering topics 3, 7, 10, 14, 15, 18, 19, 22, 24, 29, 32 and 34.

## Files and decisions assisted in this revision

- `src/lexer.c`, `src/transition_table.c`, `src/semantic_actions.c`: `YYerror` integration, valid-symbol registration, numeric actions, reversed-comparator tokenization, EOF documentation and checked conversion.
- `src/symbol_table.c`, `include/symbol_table.h`: provisional INTEGER magnitude, stable borrowed references, cleanup after parsing and allocation checks.
- `src/diagnostics.c`, `include/diagnostics.h`, `src/main.c`: origin-based recovery, counted pending diagnostics, final flushing and exit status.
- `src/grammar.y`: unified attribute assignments, operand diagnostics, header recovery and conflict-free productions.
- `build.sh`, `run_tests.sh`, `scripts/run_tests.py`, `tests/`: conflict enforcement, executable assertions and added regression cases.
- `README.md`, `docs/` and `scripts/build_report.py`: technical explanations and documentation generated from the actual matrices.

## Verification and defense

AI output was checked by compiling with warnings as errors, rejecting every Bison shift/reduce and reduce/reduce conflict, executing both valid and invalid fixtures, inspecting token sequences and final symbol tables, and running sanitizer checks. The tests cover independent errors on a single line and errors in headers and bodies, rather than relying on a generic failure status.

The team remains responsible for understanding and defending each implementation choice. The report distinguishes historical attribution, current changes and observed verification; it does not claim AI authorship alone proves correctness.
