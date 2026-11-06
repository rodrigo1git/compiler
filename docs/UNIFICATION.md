# Una sola semántica del compilador

Implementación del 6 de noviembre de 2026. Se retiró la compatibilidad histórica
TP3: todas las opciones realizan los mismos análisis y especialización.
`--analyze` oculta la polaca, `--rpn` la muestra y `--wasm` agrega la emisión WAT.
Las etapas de los trabajos prácticos siguen identificando sus requisitos; no son
versiones alternativas del lenguaje.

## Implementación

- Se eliminaron `checked_numeric`, `comptime_enable_tp4` y
  `comptime_tp4_enabled`. La evaluación usa siempre `numeric_binary` y
  `numeric_integer`; se retiraron las operaciones históricas duplicadas.
- `main` comprueba los cuerpos fuente, especializa y comprueba los cuerpos
  ejecutables antes de seleccionar la salida. Los tipos dependientes de índices
  comptime se comprueban también sin generar WAT.
- La auditoría de código descartado, la resolución de índices ordinarios con
  capturas y el reconocimiento de reentradas se aplican siempre.
- La contención recursiva de objetos se rechaza en el chequeo común, incluso
  sin instancias. El recorrido memoriza clases verificadas; un grafo compartido
  de 36 clases se analizó en 0,005 segundos en la comprobación manual.
- `codegen_attribute_assignment` valida sus entradas al comienzo y delega la
  conservación del índice de destino. Los atributos constantes se resuelven una
  sola vez. La validación de llamadas termina antes de emitirlas.
- La emisión de operadores con línea y las reservas comprobadas se centralizaron.
  `returns_object` se eliminó de codegen: la prohibición se comprueba en types.
  `codegen.c` pasó de 548 a 496 líneas. Se mantienen las comprobaciones que
  permiten recuperar errores del parser y liberar fragmentos incompletos.

No se modificaron los controles cdgi, la gramática ni las reglas confirmadas de
copias, ámbitos, argumentos y FROM. Se conservaron las correcciones de receptores
anidados, ceros con signo y argumentos comptime por iteración.
La validación de plantillas sin invocar sigue excluida por indicación del usuario.

## Expectativas comunes

Los casos de cátedra expresan ahora una sola expectativa. Se retiraron los mapas
de excepciones por nombre de `verify_tp4.py`; `compiler_cases.py` reúne los casos
para los verificadores de salidas y ejecución.

| Situación | Resultado común |
| --- | --- |
| Operación, argumento, control FROM o retorno de tipo incompatible | Error de compilación, también al pedir solamente polaca. |
| TOI ordinario que pierde fracción | Compila; el ejecutable falla por g. |
| TOI fraccionario necesario para una asignación comptime | Error de compilación. |
| Suma INTEGER 32767 + 1 | Normalización int16: −32768, también durante evaluación comptime. |
| Reentrada recursiva, incluso con argumentos comptime diferentes | Compila; el ejecutable falla por i antes de reingresar al cuerpo. |
| Lectura mediante índice ordinario de un objeto con campos comptime | Captura de los campos y comprobación de todos los candidatos de tipo. |

Estas reglas ya pertenecían al recorrido Wasm anterior. Cambian deliberadamente
las expectativas del recorrido histórico que se eliminó.

El evaluador Python se utiliza para estructura de IR y operaciones que puede
representar fielmente. Los archivos `tp3_calls.txt` y `tp3_control.txt` pasan a
verificarse en Wasm real: el primero imprime 10, 8, 19, 7, 7, 8 y falla por i;
el segundo imprime 15, 24, 24 y falla por g. Sus antiguas expectativas Python
permitían recursión o truncamiento que el destino ya controlaba.

## Verificación

El binario de referencia se construyó antes de los cambios y se conservó aparte.
Su SHA-256 es `a7ec76e61aa3fb7854df0f7ccef9948ebdeeeecc0b485b4c3744a7ae71496053`.

- Construcción estricta: GCC con `-Wall -Wextra -Werror` y Bison sin conflictos.
- Inventario: 319 definiciones documentadas con enlaces vigentes.
- Verificador de diagnósticos: siete pruebas propias.
- Análisis y polaca: 118 comprobaciones y 172 casos de cátedra, 344 ejecuciones.
  Los dos archivos trasladados a Wasm explican la diferencia respecto de las
  120 comprobaciones históricas; sus programas siguen cubiertos.
- Consistencia: 324 escenarios con salida predeterminada, `--rpn`, `--analyze`,
  `--wasm` y `--analyze --wasm`, más el binario de referencia: 1944 invocaciones.
  No cambian los diagnósticos entre salidas ni respecto de la referencia Wasm.
  La polaca coincide y los 218 programas aceptados generan WAT idéntico byte
  por byte al de referencia.
- Ejecución real mediante WABT y Node: 324 escenarios, incluyendo errores de
  compilación, controles runtime y ciclos no terminantes comprobados por timeout.
- Las suites se repiten con Clang y ASan/UBSan, usando
  `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` y `UBSAN_OPTIONS=halt_on_error=1`.
  Esta intervención no declara comprobación de fugas ni de navegador.
- La auditoría independiente conserva 31/33 coincidencias; las dos discrepancias
  restantes corresponden a las plantillas sin invocar excluidas del alcance.

Reportes de archivos finales:

| Comprobación | Normal | ASan/UBSan |
| --- | --- | --- |
| Opciones y comparación con referencia | [salidas](unification-outputs.json) | [salidas](unification-outputs-sanitized.json) |
| Casos de cátedra | [cátedra](unification-course.json) | [cátedra](unification-course-sanitized.json) |
| Compilación y ejecución Wasm | [Wasm](unification-wasm.json) | [Wasm](unification-wasm-sanitized.json) |

Los reportes Wasm incluyen hashes de fuentes, verificadores y binarios. Los
reportes anteriores se conservan como evidencia histórica y no describen la
selección de salidas vigente.
