# Separación del especializador

Refactor del 6 de noviembre de 2026, sin cambios de semántica ni de la interfaz
de `specialize.h`. El archivo de 1282 líneas se dividió por responsabilidad:

| Archivo | Líneas | Responsabilidad |
| --- | ---: | --- |
| `specialize.c` | 168 | Coordina la sesión, registra fuentes, expone consultas e imprime resultados. Incluye las utilidades internas compartidas de reserva, búsqueda y descripción de controles. |
| `specialize_effects.c` | 137 | Calcula y propaga resúmenes de escrituras y aplica los efectos de llamadas. |
| `specialize_instances.c` | 158 | Prepara activaciones, mantiene la caché y crea/libera cuerpos especializados. |
| `specialize_probe.c` | 330 | Explora requisitos por iteración sobre entornos privados. |
| `specialize_transform.c` | 553 | Transforma instrucciones, llamadas, atributos y controles en polaca ejecutable. |

La separación agrega declaraciones internas y funciones con contratos explícitos;
el total es 1346 líneas C más 85 de cabecera interna. El objetivo es facilitar la
lectura y localizar las reglas, conservando el comportamiento comprobado.

## Estado y propiedad

`specialize_state_t` agrupa fuentes, instancias, cuerpo principal, contador de
nombres, profundidad y presupuesto. Sólo el coordinador mantiene la sesión;
los otros módulos reciben su dirección explícitamente. La interfaz pública sigue
representando una compilación por proceso; el cambio no declara reentrante al
compilador completo ni a su tabla de símbolos.

El registro conserva referencias prestadas a símbolos y cuerpos fuente. Cada
instancia posee su polaca y sus entornos inicial/final. El módulo de instancias
libera esos recursos; el coordinador libera principal y registro de fuentes.
Los contextos de transformación poseen sus pilas y llamadas pendientes. Los
contextos de exploración poseen sus entornos y capturas temporales.

`specialize_internal.h` se incluye únicamente desde los cinco módulos. Comparte
los tipos necesarios y doce operaciones internas con prefijo `sp_`. Las funciones
de recorrido y sus estructuras permanecen privadas en cada archivo. `specialize.h`
se conserva byte por byte, y los consumidores externos no requieren cambios.

## Simplificaciones aplicadas

- `sp_apply_effects` es la única implementación de propagación de efectos al
  llamador. Se utiliza también sobre los entornos privados del explorador.
- `probe_step` delega operadores a `probe_operator`, que separa la asignación en
  `probe_assign`. Cada operación conserva la propiedad y liberación de operandos.
- `probe_control` delega a `probe_if` y `probe_loop`: la unión de ramas y la
  exploración de iteraciones tienen recorridos separados.
- `resolve_attribute` resuelve receptor concreto, índice, campo y captura antes
  de preparar el destino o la lectura mediante `process_attribute`.
- Los límites existentes se nombran en la cabecera interna: 128 niveles y
  100000 unidades del presupuesto correspondiente. No se agregaron modos.

La creación de instancias llama al transformador y éste puede solicitar nuevas
instancias. Esa recursión del algoritmo sigue controlada por las instancias
activas y los límites de recursos. El explorador consulta fuentes y efectos;
no publica instrucciones, especializaciones ejecutables ni diagnósticos de un
intento descartado.

## Validación

Se conservó un binario de referencia antes de cambiar el código y se compararon
también las etapas intermedias de la extracción. Los archivos finales pasaron:

- Build GCC con `-Wall -Wextra -Werror`, Bison sin conflictos.
- 328 definiciones C documentadas una vez con enlaces vigentes.
- Siete pruebas del verificador de diagnósticos.
- 118 comprobaciones de análisis/polaca y 172 escenarios de cátedra:
  344 ejecuciones aprobadas.
- 324 escenarios con cinco selecciones de salida y el binario de referencia:
  1944 invocaciones. La polaca coincide entre salidas y no cambian los diagnósticos
  respecto de la referencia. Los 218 programas aceptados generan WAT idéntico
  byte por byte al anterior al refactor.
- 324 escenarios de compilación y ejecución real con WABT y Node: 180 normales,
  34 errores runtime esperados, 106 rechazos de compilación y cuatro ciclos no
  terminantes comprobados por timeout.
- Las mismas suites pasan con Clang y ASan/UBSan, usando
  `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` y `UBSAN_OPTIONS=halt_on_error=1`.
  No se declara verificación de fugas ni de navegador en esta intervención.

La validación de plantillas sin invocar continúa fuera del alcance solicitado.
No se modificaron las expectativas de las pruebas para este refactor.

[Reporte de archivos finales](specialize-refactor-validation.json): hashes del
binario de referencia, binarios finales, fuentes y verificadores; escenarios y
resultados normales/con sanitizadores. `git diff --check` también pasó.
