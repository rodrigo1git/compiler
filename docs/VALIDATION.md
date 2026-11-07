# Validación del backend TP4

La intervención más reciente comparte controles RPN y evaluación, corrige la
captura de índices comptime y reorganiza memoria/backend. Su descripción está en
[SHARED_EVALUATION_REFACTOR.md](SHARED_EVALUATION_REFACTOR.md) y sus resultados
normales y ASan/UBSan en [shared-evaluation-validation.json](shared-evaluation-validation.json).
La separación previa del especializador está en [SPECIALIZE_REFACTOR.md](SPECIALIZE_REFACTOR.md).
La eliminación previa de compatibilidad histórica TP3 está en [UNIFICATION.md](UNIFICATION.md).
La corrección previa de H1–H3 está registrada en [CORRECTIONS.md](CORRECTIONS.md).
Ambas tienen reportes separados y sus límites de validación. El texto siguiente conserva la evidencia
histórica del 5 de noviembre; sus herramientas y afirmaciones sobre navegador
y fugas no describen la ejecución del día 6.

Validación del 5 de noviembre de 2026 sobre la rama code-generation,
con cambios locales posteriores a 16af00b. Los hashes exactos del binario y
fuentes comprobados están en validation.json y validation-sanitized.json.
Los reportes incluyen también los hashes de los verificadores y casos de prueba.
El código no fue commiteado ni publicado como parte de esta revisión.

## Resultados

- Build: Bison sin conflictos shift/reduce o reduce/reduce; GCC con
  -Wall -Wextra -Werror.
- TP3: 120 comprobaciones del verificador original ampliado.
- Ejemplos de cátedra TP3: 172 escenarios, 344/344 ejecuciones del compilador.
- TP4: 282 escenarios. 148 ejecuciones normales, 31 errores runtime esperados,
  102 rechazos de compilación y un ciclo no terminante comprobado con timeout.
- Verificador TP4: siete pruebas propias; los rechazos exigen su diagnóstico
  esperado y los errores de sanitizadores no se aceptan como errores del fuente.
- Inventario: 306 funciones C documentadas individualmente, con contrato,
  requisitos y enlace a la definición actual. El verificador comprueba cobertura,
  unicidad y líneas; no decide si la explicación es correcta.
- Comprobaciones adicionales de protección de la entrada y fallos de escritura.
- WABT valida los módulos y Node ejecuta el Wasm real.
- Las tres suites también pasaron con AddressSanitizer, UndefinedBehaviorSanitizer
  y LeakSanitizer, detect_leaks=1 y halt_on_error=1, sobre el código final.
- git diff --check terminó sin errores.

La prueba tests/verify_tp4_browser.py se ejecutó en esta revisión: el selector
real del lanzador produjo 3, 7, 3 mediante Chromium headless. Los sanitizadores
también se ejecutaron, incluida la detección de fugas.

Herramientas observadas: GCC 16.2.1, Bison 3.8.2, Node 26.12.0, WABT 1.0.39.

## Revisión manual de resultados

Se compilaron fuentes temporales, se inspeccionaron los diagnósticos y el WAT,
y se ejecutaron los módulos mediante WABT y Node. Las expectativas se contrastaron
con el programa fuente. El evaluador Python de IR no sustituyó al programa destino.

Se corrigieron dos fallos reproducidos en la auditoría: la lectura de un atributo
comptime como índice perdía el receptor del método, y los ciclos descartados sólo
se revisaban en su primera iteración. Se agregaron 17 escenarios de regresión,
incluidos receptores distintos, herencia, escrituras, retornos descartados,
iteraciones posteriores, ciclos anidados y conservación de los valores reales.

| Caso revisado manualmente el 5 de noviembre | Resultado observado |
| --- | --- |
| Dos receptores seleccionan campos diferentes y uno modifica el segundo | 3, 7, 9, 3, 9; la escritura mantiene separado el primer campo. |
| Índice comptime heredado de la segunda base | 3, 7; se usa el receptor real aunque cambie la posición del índice. |
| Incompatibilidad recién en la segunda o tercera iteración descartada | Error INTEGER := SINGLEF en la línea 10; salida 1 y ningún WAT nuevo. |
| Índice calculado por dos ciclos descartados anidados | Error INTEGER := SINGLEF en la línea 11, al seleccionar el tercer atributo. |
| 128 copias de objetos dentro de un ciclo posterior a RET | 0, 3, 7, 9; el análisis conserva original, copia e índice reales. La función ejecutable sólo devuelve 0. |
| Atributo índice de un método después de RET | Índice 0: devuelve 0. Índice 1: error de retorno SINGLEF con línea; ningún WAT nuevo. |

La revisión anterior del 4 de noviembre descubrió tres fallos en el chequeo:
una llamada simple perdía su retorno conocido, una escritura no relacionada
invalidaba el índice y una copia de objeto podía conservar el valor viejo de un
campo. Los dos primeros rechazaban casos válidos; el tercero podía admitir un
retorno SINGLEF en una función INTEGER. Se corrigieron y se incorporaron regresiones
válidas e inválidas, incluidas copias de argumentos y aislamiento de efectos.

| Caso de la última revisión | Resultado observado |
| --- | --- |
| Retorno SINGLEF seleccionado por índice comptime en IF falso, ELSE eliminado, después de RET y en FROM vacío | Error de tipo con línea; salida 1 y ningún WAT nuevo. |
| Las variantes anteriores seleccionando INTEGER | Compilan; ejecutan y devuelven 0. |
| Contexto válido con índice 0 seguido de contexto inválido con índice 1 | Se rechaza el segundo contexto; la caché no oculta el error. |
| f(200) llama a f(p-1) o a f(p+1) | Compila y ensambla; imprime sólo 200 y falla por i. No imprime 999. |
| Recursión indirecta con parámetros comptime distintos | Imprime 1, 2 y falla por i antes de volver a entrar a la función exterior. |
| Caso base y llamadas anidadas sin reentrada | 0, 0; llamadas ordinarias sucesivas/anidadas: 3, 4. |
| Índice devuelto por una función dentro de código descartado | El retorno conocido se conserva: índice 0 válido; índice 1 incompatible. |
| Llamada descartada que escribe otra variable | 0, 7; el índice sigue conocido y la variable real conserva 7. |
| Llamada descartada que cambia un índice comptime de 1 a 0 | 0, 1; el chequeo usa 0, el programa real conserva 1. |
| Copia descartada que cambia campos comptime | Caso válido: 0, 1, 1; original y copia reales conservan 1. El caso que copia índice 1 se rechaza por SINGLEF. |
| Objeto pasado por copia en una llamada descartada | Caso válido: 0, 1; el índice 1 incompatible se rechaza. |
| Dos retornos incompatibles en líneas diferentes | Aparecen ambos diagnósticos, con las líneas de sus operaciones. |
| Llamada descartada que imprimiría 888 y desbordaría un producto | Sólo imprime 7; la llamada no se ejecuta. |
| Llamada recursiva después de RET | Dos llamadas independientes imprimen 200, 201 sin activar la guardia. |

En el WAT de f(200) se comprobó una única especialización: la llamada recursiva
apunta a ella y la guardia de la declaración fuente aparece antes del primer POUT.
El segundo ingreso falla antes de ejecutar constantes del cuerpo especializado.

Los siguientes resultados corresponden a la revisión manual anterior del backend;
sus programas siguen cubiertos por las suites ejecutadas sobre la versión final:

| Programa | Resultado observado | Interpretación |
| --- | --- | --- |
| tp3_comptime_specialization | 3, 7, 3 | La reutilización vuelve al valor capturado 3. |
| tp3_comptime_copies | 10, 16, 11, 16, 3, 7, 14 | Copias, campos hermanos y orden de argumentos mantienen valores independientes. |
| inherited_second_base | 7, 22, 11 | El método heredado lee y en la clase real sin confundirlo con x de la primera base. |
| destination_index_snapshot | 9, 7 | La llamada modifica el índice original, pero la asignación conserva el destino elegido. |
| calls_successive_nested | 3, 4 | Preparar una llamada exterior no activa prematuramente la guardia de recursión. |
| float_overflow_negative | imprime 9 y falla por c | No ejecuta el POUT posterior de 99. |
| product_from_call | falla por d dentro de la función | El control corresponde a int16 y se ejecuta dentro del módulo. |
| conversion_negative_fraction | imprime 9 y falla por g | No trunca silenciosamente −3.9. |
| recursion_indirect | falla por i | La identidad fuente detecta reentrada mediante una función anidada. |
| invalid_destination_before_rhs | imprime 7 y falla por índice | La llamada del RHS no imprime 88. |

La representación del control es estructurada: block/loop/if y salidas al epílogo.
Los campos se ubican mediante identidad de atributo y header del receptor. Los
auxiliares visibles en TS corresponden a locales del Wasm; las capturas hijas se
aplican a copias de objetos, no al original.

## Reproducción

```sh
npm ci --prefix runtime
./tests/run_all.sh --report /tmp/tp4-report.json
python3 tests/verify_tp4_browser.py
```

Para sanitizadores, después de generar el parser:

```sh
gcc -Wall -Wextra -Werror -std=gnu99 -g -O1 \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Iinclude -I. src/*.c y.tab.c -lm -o /tmp/compiler-sanitized
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 tests/verify_tp3.py /tmp/compiler-sanitized
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 tests/verify_tp3_course.py /tmp/compiler-sanitized
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  python3 tests/verify_tp4.py --compiler /tmp/compiler-sanitized
```

Las restricciones de memoria y las decisiones donde el enunciado deja margen
están documentadas en TP4.md. Estos resultados constituyen evidencia de los casos
verificados; no una demostración de ausencia de cualquier error posible.
