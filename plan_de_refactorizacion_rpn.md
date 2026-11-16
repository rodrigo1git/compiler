# Plan de Refactorización y Limpieza de Código: Subsistema RPN (`rpn`, `codegen`, `types`, `specialize`)

Este documento define el plan verificado para refactorizar la generación, representación, validación y transformación de Polaca Inversa (RPN), aplicando buenas prácticas de programación en C y principios de *Clean Code* (siguiendo la línea de [plan_de_refactorizaci_n_para_wasm_emit.md](file:///home/rodrigo/facultad/compi/plan_de_refactorizaci_n_para_wasm_emit.md)), sin alterar ningún byte de las salidas del compilador (`stdout`, `stderr`, tabla de símbolos ni `.wat`).

---

## 1. Principios de Ingeniería en C Aplicados

1. **Encapsulamiento de Tipo de Datos Abstracto (TDA):**
   * Basado en *C Interfaces and Implementations* (D. Hanson): el módulo propietario de una estructura (`rpn.c`) debe ofrecer constructores y mutadores completos que mantengan sus invariantes. Ningún módulo externo debe escribir directamente en `rpn->steps[i]` para completar una instrucción a medias ni hacer aritmética manual entre direcciones RPN (base 1) e índices C (base 0).
2. **Un Solo Nivel de Abstracción por Función (SLAP) y Funciones Cohesivas:**
   * Los despachadores (`switch (step->kind)` y `switch (op)`) sólo deben coordinar el flujo y delegar cada caso complejo en funciones `static` pequeñas con nombres semánticos claros y un máximo de 3 niveles de indentación.
3. **Predicados con Nombre y Cláusulas de Guarda (*Early Returns*):**
   * Reemplazar condiciones booleanas extensas de múltiples líneas (`&&` y `||` anidados) por funciones predicado `static bool` que expresen directamente la regla del lenguaje.
4. **Legibilidad Horizontal y Depurabilidad en `gdb`/`lldb`:**
   * **Máximo 1 sentencia ejecutable por línea:** eliminar líneas compactadas del tipo `case X: stmt1; stmt2; break;` o `if (cond) { a; b; }`.
   * **Bloques explícitos con llaves `{}`** y eliminación de operadores ternarios anidados.
5. **DRY Conservador (Sin Acoplamiento Destructivo):**
   * Unificar código idéntico (emisión/parcheo de saltos `BF`/`BI`, lectura numérica de literales con exponente `'s'`), pero mantener separados los recorridos cuyas políticas semánticas o diagnósticos difieren.

---

## 2. Diagnóstico Verificado y Correcciones al Plan Inicial

### 2.1. Problemas Confirmados en el Código Actual

| Problema / Code Smell | Ubicación Exacta | Impacto |
| :--- | :--- | :--- |
| **API de `rpn.h` incompleta y código muerto** | [include/rpn.h](file:///home/rodrigo/facultad/compi/include/rpn.h#L82-L86), [src/rpn.c](file:///home/rodrigo/facultad/compi/src/rpn.c#L44-L61) | `rpn_append_symbol`, `rpn_append_destination` y `rpn_reserve_target` no reciben `source_line`. `rpn_append_operator` no tiene ningún llamador en todo el proyecto. |
| **Fuga de encapsulamiento e indexación manual `target - 1` / `count + 1`** | [src/codegen.c](file:///home/rodrigo/facultad/compi/src/codegen.c#L298-L330), [src/codegen.c](file:///home/rodrigo/facultad/compi/src/codegen.c#L419-L488), [src/specialize_transform.c](file:///home/rodrigo/facultad/compi/src/specialize_transform.c#L115-L188) | Los llamadores modifican `steps[0]`, `steps[count - 1]` y `steps[target - 1]` desde afuera tras insertar un paso. |
| **Campo muerto `bool loop` en `rpn_step_t`** | [include/rpn.h](file:///home/rodrigo/facultad/compi/include/rpn.h#L54), [src/codegen.c](file:///home/rodrigo/facultad/compi/src/codegen.c#L475), [src/specialize_transform.c](file:///home/rodrigo/facultad/compi/src/specialize_transform.c#L175) | Se asigna `true` en dos lugares y **nunca se lee**; `rpn_describe_control` ([src/rpn.c](file:///home/rodrigo/facultad/compi/src/rpn.c#L184-L186)) deduce si es ciclo verificando el salto `BI` hacia `start`. |
| **Emisión de saltos `IF`/`ELSE`/`FROM` duplicada a bajo nivel** | [src/codegen.c](file:///home/rodrigo/facultad/compi/src/codegen.c#L419-L489) y [src/specialize_transform.c](file:///home/rodrigo/facultad/compi/src/specialize_transform.c#L115-L188) | Ambos módulos repiten la secuencia de reservar `RPN_TARGET`, cargar `control_start`/`loop_variable`, emitir `BF`/`BI` y parchear a `count + 1`. |
| **Conversión de literales numéricos duplicada** | `constant_value` en [src/codegen.c](file:///home/rodrigo/facultad/compi/src/codegen.c#L447-L456) vs `comptime_read_constant` en [src/comptime_values.c](file:///home/rodrigo/facultad/compi/src/comptime_values.c#L102-L115) | Repite reserva dinámica, reemplazo `'s' -> 'e'` y `strtod`. |
| **Funciones monolíticas con `switch` gigantes y sentencias apiladas** | `check_body` ([src/types.c](file:///home/rodrigo/facultad/compi/src/types.c#L88-L162)), `resolve_step` ([src/types.c](file:///home/rodrigo/facultad/compi/src/types.c#L235-L330)), `rpn_print` ([src/rpn.c](file:///home/rodrigo/facultad/compi/src/rpn.c#L122-L168)), `analyze_function_effects` ([src/specialize_effects.c](file:///home/rodrigo/facultad/compi/src/specialize_effects.c#L69-L102)) | Mezclan niveles de abstracción, ternarios anidados y múltiples sentencias en una misma línea. |

### 2.2. Correcciones Críticas Detectadas en la Reverificación

> [!IMPORTANT]
> **Por qué NO se deben fusionar destructivamente `probe_copy_object` y `comptime_copy_object_fields` ni `resolve_step` y `probe_step`:**
> 1. **`probe_copy_object` ([src/specialize_probe.c](file:///home/rodrigo/facultad/compi/src/specialize_probe.c#L79-L133)) vs `comptime_copy_object_fields` ([src/comptime_objects.c](file:///home/rodrigo/facultad/compi/src/comptime_objects.c#L158-L203)):**
>    * `probe_copy_object` actúa tanto en modo exploración silenciosa (donde marca `probe->needs_iteration = true` sin reportar error) como en validación (`probe->validating`), y utiliza el mensaje `"Comptime attribute '%s' has no known value when copying an object '%s'"` sobre instantáneas ya copiadas en `probe_push`.
>    * `comptime_copy_object_fields` distingue entre copia de argumento (`"in argument copy for object"`) y asignación ordinaria (`"Value for comptime '%s' is not known at compile time in object"`), y captura los hermanos antes de escribir para soportar auto-asignación (`a := a`).
>    * **Decisión:** Conservar ambas funciones con sus contratos exactos, limpiando su estructura interna y legibilidad sin acoplarlas.
> 2. **`types_check_discarded` ([src/types.c](file:///home/rodrigo/facultad/compi/src/types.c#L235-L441)) vs `specialize_probe` ([src/specialize_probe.c](file:///home/rodrigo/facultad/compi/src/specialize_probe.c#L235-L388)):**
>    * `types_check_discarded` muta una copia local de `rpn_t` (reemplazando índices `comptime` por constantes) para auditar tipos en ambas ramas de un `IF` y en ciclos hasta punto fijo.
>    * `specialize_probe` evalúa llamadas y necesidad de iteración sin mutar IR y deteniéndose en `RET`.
>    * **Decisión:** Mantener separados ambos recorridos, descomponiendo sus `switch` internos en funciones `static` cohesivas.

---

## 3. Invariantes que No Pueden Alterarse

1. **Formato y numeración de la Polaca Inversa (`--- RPN ---`, `--- FUNCTION ... ---`, `--- TEMPLATE ... ---`):**
   * Los pasos siguen numerándose en base 1 (`1..count`), y `count + 1` sigue siendo la dirección de salida al final del cuerpo.
   * Cada salto condicional o incondicional sigue ocupando dos pasos consecutivos en el arreglo: `RPN_TARGET` seguido de `RPN_OPERATOR` (`BF` o `BI`).
2. **Orden y contenido de la Tabla de Símbolos:**
   * El orden y las claves de inserción en `symbol_table` (`@index%u`, `@value%u`, `%s$ct%u`, `@tmp_%zu_%zu`, `0$i`, `0.0`) deben mantenerse idénticos para no alterar el layout de memoria ni los IDs en `.wat`.
3. **Diagnósticos y códigos de salida:**
   * Los mensajes en `stderr` y sus números de línea deben coincidir exactamente en los 332+ escenarios de prueba.
4. **Inventario de funciones (`docs/FUNCTIONS.md`):**
   * `python3 tests/verify_function_docs.py` exige que cada función definida en `src/*.c` y `src/*.y` tenga exactamente una fila en `docs/FUNCTIONS.md` apuntando a su línea exacta `#L<n>`.

---

## 4. Plan de Ejecución por Fases

### Fase 0: Línea Base de Verificación (Baseline)
1. Copiar el binario actual `./compiler` a `/tmp/compiler_baseline`.
2. Al finalizar cada fase, validar:
   * `python3 tests/verify_outputs.py --baseline /tmp/compiler_baseline`
   * `./tests/run_all.sh`

---

### Fase 1: Encapsulamiento y Limpieza del Módulo `rpn` ([include/rpn.h](file:///home/rodrigo/facultad/compi/include/rpn.h), [src/rpn.c](file:///home/rodrigo/facultad/compi/src/rpn.c))
* **Objetivo:** Convertir `rpn_t` en un TDA completo donde ningún llamador externo tenga que manipular `steps[i]` tras insertar ni hacer cuentas `target - 1` o `count + 1`.
* **Acciones:**
  1. Eliminar el campo muerto `bool loop;` de `rpn_step_t` en [include/rpn.h](file:///home/rodrigo/facultad/compi/include/rpn.h#L54).
  2. Definir `RPN_INITIAL_CAPACITY` (16) en `src/rpn.c`.
  3. Reemplazar las funciones incompletas (`rpn_append_symbol`, `rpn_append_destination`, `rpn_append_operator`, `rpn_reserve_target`) por emisores completos con `int line`:
     ```c
     size_t rpn_emit_symbol(rpn_t *rpn, const map_node_t *symbol, int line);
     size_t rpn_emit_destination(rpn_t *rpn, const map_node_t *symbol, int line);
     size_t rpn_emit_operator(rpn_t *rpn, rpn_operator_t op, int line);
     size_t rpn_emit_branch_false(rpn_t *rpn, size_t control_start,
                                  const map_node_t *loop_variable, int line);
     size_t rpn_emit_branch(rpn_t *rpn, size_t target, int line);
     bool rpn_patch_to_end(rpn_t *rpn, size_t address);
     ```
  4. En `src/rpn.c`, extraer de `rpn_print` funciones auxiliares claras (`print_call_step`, `print_attribute_step`) eliminando los ternarios anidados y dejando 1 sentencia por línea.

---

### Fase 2: Modularización y Limpieza de `codegen.c` ([src/codegen.c](file:///home/rodrigo/facultad/compi/src/codegen.c))
* **Objetivo:** Hacer que `codegen.c` exprese directamente la traducción de construcciones del lenguaje a RPN sin manipular índices internos del arreglo.
* **Acciones:**
  1. Eliminar `append_operator` local y usar `rpn_emit_operator`, `rpn_emit_symbol`, `rpn_emit_destination`.
  2. Simplificar `codegen_if_begin`, `codegen_if_else`, `codegen_if_end`, `codegen_for_begin` y `codegen_for_end` usando `rpn_emit_branch_false`, `rpn_emit_branch` y `rpn_patch_to_end` (eliminando `append_branch` y todos los accesos `current_sequence->steps[... - 1]`).
  3. Reemplazar `constant_value` ([src/codegen.c](file:///home/rodrigo/facultad/compi/src/codegen.c#L447-L456)) por `comptime_read_constant(symbol).number`, eliminando la duplicación del parseo de exponentes `'s'`.
  4. Desempaquetar todas las líneas con múltiples sentencias en `validate_evaluation_order`, `codegen_function_abort` y helpers de llamadas.

---

### Fase 3: Descomposición de `types.c` ([src/types.c](file:///home/rodrigo/facultad/compi/src/types.c))
* **Objetivo:** Dividir los dos bloques monolíticos (`check_body` y `resolve_step`) en funciones cohesivas de un solo nivel de abstracción y desempaquetar el formato compacto.
* **Acciones:**
  1. Extraer de `check_body` ([src/types.c](file:///home/rodrigo/facultad/compi/src/types.c#L88-L162)):
     * `check_attribute_step(const rpn_step_t *step, int line)`
     * `check_operator_step(const rpn_step_t *step, const map_node_t *function, type_t *stack, size_t *count, int line)`
  2. Extraer de `resolve_step` ([src/types.c](file:///home/rodrigo/facultad/compi/src/types.c#L235-L330)):
     * `resolve_attribute_step(type_resolution_t *context, rpn_step_t *step)`
     * `resolve_operator_step(type_resolution_t *context, const rpn_step_t *step)`
  3. Desempaquetar las sentencias múltiples por línea en `type_of_symbol`, `types_compatible`, `check_body` y `resolve_loop`.

---

### Fase 4: Limpieza de `specialize_effects.c`, `specialize_probe.c` y `specialize_transform.c`
* **Objetivo:** Unificar el uso de las primitivas de salto RPN en el transformador, reemplazar condiciones booleanas crípticas por predicados con nombre y desempaquetar líneas compactas.
* **Acciones:**
  1. En [src/specialize_transform.c](file:///home/rodrigo/facultad/compi/src/specialize_transform.c#L115-L188):
     * Usar `rpn_emit_branch_false`, `rpn_emit_branch` y `rpn_patch_to_end` dentro de `process_if` y `process_loop` (eliminando la manipulación manual de `context->output->steps[false_target - 1]`).
     * Desempaquetar las sentencias de una línea en `process_if`, `process_loop`, `evaluate_binary`, `assign_value` y `process_operator`.
  2. En [src/specialize_effects.c](file:///home/rodrigo/facultad/compi/src/specialize_effects.c#L41-L102):
     * Extraer predicados semánticos (`step_has_comptime_effect`) y formatear con bloques claros de 1 sentencia por línea.
  3. En [src/specialize_probe.c](file:///home/rodrigo/facultad/compi/src/specialize_probe.c):
     * Desempaquetar sentencias compactas y mantener bloques uniformes con el resto del subsistema.

---

### Fase 5: Sincronización de `docs/FUNCTIONS.md` y Validación Integral
* **Acciones:**
  1. Actualizar las entradas y números de línea en [docs/FUNCTIONS.md](file:///home/rodrigo/facultad/compi/docs/FUNCTIONS.md).
  2. Ejecutar `python3 tests/verify_function_docs.py`.
  3. Ejecutar `python3 tests/verify_outputs.py --baseline /tmp/compiler_baseline` (garantizando `0` diferencias en Polaca, Tabla de Símbolos, diagnósticos y `.wat`).
  4. Ejecutar `./tests/run_all.sh` y `git diff --check`.
  5. Eliminar `/tmp/compiler_baseline`.
