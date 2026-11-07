# Controles y evaluación compartidos

Se guardó una copia completa previa, incluyendo Git, fuentes, ejecutable y archivos
locales, en `../compiler_backup_20261106_222453`.

## Destinos

Todos los índices variables de destinos se capturan antes de evaluar el lado
derecho. El temporal de un índice comptime es un auxiliar interno comptime: su
asignación captura un valor conocido y no representa una escritura observable del
programa. El especializador comprueba el índice y elimina su almacenamiento runtime.
Los índices ordinarios conservan temporal y CHECK_INDEX anterior al RHS.

## Controles de Polaca

`rpn_control_t`, `rpn_describe_control` y `rpn_index_controls` pertenecen a RPN.
Transformación, evaluación abstracta, chequeo de fuente descartada y Wasm usan
ese contrato. Los índices se construyen sobre cuerpos completos y se liberan con
el contexto consumidor. Las posiciones de Polaca siguen comenzando en uno.

## Evaluación y validación

`comptime_call_environment`, `comptime_select_field` y
`comptime_validate_assignment` concentran reglas compartidas. El coordinador
mantiene una pila explícita de llamadas abstractas para cortar reentradas.
`specialize_evaluate_call` usa el evaluador de probe sobre entornos privados y
valida los tipos dependientes de valores mediante el resolvedor de types. No
publica instancias ni marca una plantilla como ejecutada. Las llamadas reales
siguen utilizando specialize_instances y su caché original.

La exploración de necesidades de iteración es silenciosa. La evaluación solicitada
para validación informa errores. Los recorridos conservan sus políticas distintas:
la auditoría de tipos visita también ramas descartadas; la evaluación de valores
sigue los caminos conocidos y une estados desconocidos. La creación de constantes
y ubicaciones canónicas usadas por la validación sigue perteneciendo a la TS y al
registro de objetos; los entornos privados no son una TS independiente.

## Memoria y backend

`memory_resize` reserva al menos un byte; `memory_zero` conserva la inicialización
a cero. Ambas terminan ante fallo. Se conservaron los NULL de recuperación del
parser y los datos opcionales. El backend separa emisión de helpers de memoria,
objetos y copia, y planificación de locales antes de emitir un cuerpo.

La optimización de las claves de caché y la validación de plantillas sin invocar
siguen fuera de este cambio.

## Regresión

Los casos nuevos cubren captura de índices ordinarios/comptime, campos comptime,
control desconocido y llamadas de validación posteriores a RET. La verificación de
salidas comprueba que esa validación no publique una instancia de helper.
Las suites finales pasan con GCC y con Clang ASan/UBSan: 118 comprobaciones
estructurales, 172 escenarios de cátedra (344 invocaciones), 332 escenarios de
consistencia entre opciones y 332 escenarios Wasm. Estos últimos incluyen 185
ejecuciones normales, 34 fallos runtime esperados, 109 rechazos y cuatro ciclos
no terminantes. Pasan siete pruebas del verificador y el inventario de 333
funciones. No se verificaron fugas ni navegador en esta intervención.

El recuento respecto de la copia previa es de 5040 a 5110 líneas C (+70), y de
5701 a 5794 incluyendo cabeceras (+93). La estimación anterior de reducción neta
resultó optimista: compartir controles y reservas eliminó duplicación, pero la
evaluación sin publicación de instancias requirió conservar comprobaciones que
antes aportaba el transformador. No se comprimió el formato para reducir números.

[Reporte final con hashes y resultados](shared-evaluation-validation.json).
