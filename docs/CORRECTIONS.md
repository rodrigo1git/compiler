# Correcciones de H1–H3, 6 de noviembre de 2026

Se implementaron los tres errores confirmados de la auditoría sobre `70bd41f`.
La validación de plantillas sin invocar (H4) quedó excluida expresamente por el
usuario. No se modificaron las expectativas originales de esa auditoría.

## Cambios y resultados observables

| Error | Corrección | Evidencia |
| --- | --- | --- |
| H1: estado compartido entre atributos objeto de distintos receptores | `comptime_resolve_object` combina receptor concreto y declaración del campo. Lecturas, destinos, copias y llamadas usan esa ubicación; la IR conserva sus símbolos originales. El chequeo aislado de tipos usa la misma resolución. | Dos objetos conservan 3 y 7 en ambos órdenes de escritura. Inicializar uno no permite leer el otro sin valor conocido. Pasan variantes con métodos anidados, herencia, tres niveles, copias, parámetros y activaciones sucesivas. |
| H2: +0 y −0 se unificaban en caché y propagación | `comptime_values_equal` distingue su signo, mantiene separados los infinitos de distinto signo y no unifica NaN. La comparación numérica del lenguaje no cambia. | Los recíprocos devuelven Infinity y −Infinity en ambos órdenes. La unión de ramas devuelve 2 y 1; `0.0 == -0.0` sigue siendo verdadero y TOI(−0) devuelve 0. |
| H3: FROM perdía el control ordinario necesario para un argumento comptime | El criterio de expansión combina efectos de escritura y necesidades por iteración. Un análisis abstracto sobre entornos privados contempla argumentos, asignaciones, orden explícito, llamadas y controles anidados. No instancia funciones ni publica código o diagnósticos exploratorios. | `f(i)` imprime 0, 1, 2 sin declarar i comptime. Pasan índices heterogéneos, llamadas y ciclos anidados, descenso, ciclos vacíos y modificaciones del control. Se preservan impresiones y errores runtime; argumentos desconocidos siguen siendo rechazados. |

El análisis previo tiene un presupuesto de 100000 pasos y limita a 128 niveles
la exploración de llamadas y copias. No ejecuta impresiones. Las expansiones
reales conservan su presupuesto existente de 100000 iteraciones y se detienen
ante un error nuevo. Un ciclo interior finito sin escrituras comptime puede
especializar argumentos bajo control exterior desconocido, manteniendo esa
restricción para cualquier escritura afectada.

Las regresiones permanentes están en [correction_cases.py](../tests/correction_cases.py).
Son 38 escenarios: 30 ejecuciones normales, cuatro rechazos de compilación,
un error runtime y tres ciclos no terminantes comprobados mediante timeout.
La comparación textual evita que JSON oculte el signo de ceros e infinitos.

## Integración de un cambio concurrente

Durante la intervención apareció una modificación de `codegen.c` que conservaba
el índice del destino también en IR TP3 y eliminaba `codegen_enable_tp4`.
Quedaron su declaración y llamada, por lo que el enlace fallaba. Se conservó
ese comportamiento, se retiraron ambas referencias y se trasladó el reinicio
del contador a `codegen_init`. El evaluador de pruebas TP3 reconoce los temporales
privados `@indexN` en el marco actual y la instrucción CHECK_INDEX. No se cambiaron
las salidas esperadas de sus programas.

Las primeras pruebas usaron una copia temporal de `codegen.c` del commit
auditado para no interrumpir la corrección. Los reportes enlazados a continuación
corresponden a los archivos finales del directorio, con esa integración incluida.

## Validación de los archivos finales

- Construcción mediante `./build.sh`: GCC con `-Wall -Wextra -Werror`, Bison
  sin conflictos shift/reduce ni reduce/reduce.
- Inventario: 316 definiciones C documentadas una vez, con enlaces vigentes.
- Verificador TP4: siete pruebas propias aprobadas.
- TP3: 120 comprobaciones; cátedra: 172 escenarios, 344/344 ejecuciones.
- TP4: 320 escenarios aprobados: 178 normales, 32 errores runtime esperados,
  106 rechazos de compilación y cuatro ciclos no terminantes.
- Los nueve contraejemplos H1–H3 coinciden con sus expectativas originales.
  La auditoría independiente termina con 31/33 coincidencias: las dos
  discrepancias restantes son exactamente H4, excluido del alcance.
- Las suites TP3, cátedra y TP4 también se ejecutan con Clang y
  `-fsanitize=address,undefined`, usando `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`
  y `UBSAN_OPTIONS=halt_on_error=1`.
- LeakSanitizer se intentó con `detect_leaks=1`: este entorno lo impide por
  ptrace. La detección de fugas no se declara verificada.
- La prueba opcional de navegador informó que falta Chromium; no se declara
  verificación de navegador para esta intervención. WABT y Node sí validan y
  ejecutan los módulos reales.

Reportes con hashes del binario, fuentes y verificadores:
[normal](corrections-validation.json) y
[ASan/UBSan](corrections-validation-sanitized.json).
La auditoría conserva sus resultados en
[casos_corregido.json](../../auditoria_2026-11-06/resultados/casos_corregido.json).

Herramientas: GCC 15.3.1, Clang 21.1.8, Bison 3.8.2, Node 22.22.2 y WABT 1.0.39.
