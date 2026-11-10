# Plan de Refactorización y Limpieza de Código: `wasm_emit.c` (Versión Adaptada)

Este documento detalla la estrategia para refactorizar `wasm_emit.c`, orientada a mejorar su legibilidad, mantenibilidad y modularidad conforme a principios de Clean Code y buenas prácticas de ingeniería de software en C, garantizando la preservación estricta de la semántica de compilación y compatibilidad con el runtime.

---

## 1. Diagnóstico de Deuda Técnica y Code Smells

| Code Smell / Antipatrón | Ubicación en `wasm_emit.c` | Impacto en el Código |
| :--- | :--- | :--- |
| **Flag `planning` / Doble pasada invisible** | `emitter_t.planning` (`plan_body` vs `write_body`), `write_line`, `new_local` | Es el elemento que más dificulta seguir el flujo de ejecución. Obliga a ejecutar el pipeline de emisión dos veces por función. |
| **Magic Numbers** | Códigos en `fail_if` (1, 2, 3, 4, 5, 6, 8), rangos de 16 bits (`-32768`, `32767`), shifts (`16`), float limit (`3.40282347e38`), offsets fijos (`offset=4` embebido en strings WAT). | Dificulta entender qué fallo de runtime se está disparando y qué estructura de memoria se accede. |
| **Duplicated Code (DRY)** | Formateo de flotantes (`strdup`, reemplazo `'s' -> 'e'`), verificación de overflow `f32`. | Lógica duplicada entre `read_symbol` y `materialize`, y chequeos redundantes en `binary`. |
| **Control Coupling** | Bucle `for (int helper = 0; helper < 3; helper++)` en `write_object_helpers`. | Usa ternarios anidados complejos para generar tres funciones distintas en el mismo bloque de código. |
| **Long Method / Mixed Abstraction Levels** | `binary`, `write_body`, `prepare_bodies`. | Mezclan generación de instrucciones básicas con chequeos de tipos, verificaciones de overflow, prólogos/epílogos de función y gestión de memoria dinámica. |
| **Formatting Compactness** | Múltiples sentencias por línea (`get_local(...)`, asignaciones, pops dobles). | Dificulta la depuración paso a paso en herramientas como `gdb` o `lldb`. |
| **God Struct** | `emitter_t` | Mezcla el estado global del módulo con el estado mutable y transitorio de la función que se está procesando en ese instante. |

---

## 2. Invariantes Críticas y Restricciones Técnicas

1. **Orden en `symbol_table` y `layout_t`:**
   * `layout_init` corre **antes** que `wasm_write` empiece a generar código, ordenando lexicográficamente los símbolos existentes.
   * Las variables temporales `@tmp_%zu_%zu` que genera `new_local` deben seguir registrándose con la misma clave, el mismo `owner`, `declaration_line`, `type_symbol` y `declared_type`.
2. **Semántica de redondeo en `binary` (`RPN_ADD` flotante):**
   * En la suma de flotantes existen dos chequeos contra `3.40282347e38`: uno en 64 bits sobre `$wide` antes de `f32.demote_f64`, y otro después de almacenar en el temporal de 32 bits. **No deben unificarse de forma destructiva**: una suma en `f64` puede no desbordar, pero al degradarse a `f32` puede convertirse en $\pm\infty$.
3. **Diferencias entre `read_symbol` y `materialize`:**
   * `read_symbol` gestiona la carga a temporales y el `snapshot` en memoria de objetos.
   * `materialize` solo emite las constantes directamente sobre la pila para un `%s.store` inmediato. La función compartida solo debe ocuparse de la emisión de la constante numérica o de cadena.
4. **Orden de los temporales:**
   * Mantener el orden estricto de llamadas a `save_local` para que los identificadores de variables locales (`$tmp_b_i`) coincidan de forma determinista.
5. **Idempotencia de `map_put`:**
   * `map_put` sobre `symbol_table` devuelve el nodo existente si ya fue insertado previamente con la misma clave y valor.

---

## 3. Plan de Ejecución por Fases (Adaptado)

### Fase 0: Red de Seguridad (Golden Tests & Baseline)
* **Objetivo:** Asegurar que ninguna transformación sintáctica altere el código WebAssembly producido ni la semántica del compilador.
* **Acciones:**
  1. Mantener un binario baseline del compilador (`compiler_baseline`).
  2. Ejecutar la verificación contra el baseline: `python3 tests/verify_outputs.py --baseline ./compiler_baseline` (compara byte a byte las 336 salidas `.wat`).
  3. Ejecutar la suite completa (`./tests/run_all.sh`) en cada fase.

---

### Fase 1: Extracción de Constantes y Nombres Semánticos
* **Objetivo:** Eliminar números mágicos del código fuente.
* **Acciones:**
  1. Definir enum semántico para los códigos de error runtime:
     ```c
     typedef enum {
         WASM_ERR_FLOAT_OVERFLOW     = 1,
         WASM_ERR_INT_OVERFLOW       = 2,
         WASM_ERR_FRACTIONAL_TRUNC   = 3,
         WASM_ERR_FLOAT_TO_INT_RANGE = 4,
         WASM_ERR_INVALID_FIELD      = 5,
         WASM_ERR_RECURSIVE_CALL     = 6,
         WASM_ERR_OUT_OF_MEMORY      = 8
     } wasm_runtime_error_t;
     ```
  2. Definir constantes para el layout del marco de activación:
     ```c
     #define FRAME_STATIC_LINK_OFFSET 0
     #define FRAME_RECEIVER_OFFSET    4
     #define FRAME_HEADER_SIZE        8
     ```
  3. Reemplazar los offsets mágicos en strings WAT:
     * `write_line(e, "i32.load offset=%d", FRAME_RECEIVER_OFFSET);` en `receiver_address`.
     * `write_line(e, "i32.store offset=%d", FRAME_RECEIVER_OFFSET);` en `begin_call`.
  4. Definir constantes para límites numéricos y de tipos:
     ```c
     #define INT16_MIN_WASM          (-32768)
     #define INT16_MAX_WASM          (32767)
     #define INT16_SIGN_EXTEND_SHIFT 16
     #define FLOAT32_MAX_ABS_STR     "3.40282347e38"
     ```
  5. Sustituir todas las apariciones literales en `wasm_emit.c`.

---

### Fase 2: Formateo y Desempaquetado de Sentencias
* **Objetivo:** Incrementar la legibilidad horizontal y facilitar depuración.
* **Acciones:**
  1. Desempaquetar sentencias compuestas en líneas individuales (máximo 1 sentencia por línea).
  2. Expandir funciones inline compactas (`body_id`, `wasm_type`, `get_local`, `push`, `pop`, `fail_if`, `integer_type`) a bloques estándar con llaves.

---

### Fase 3: Eliminación de Código Duplicado (DRY)
* **Objetivo:** Centralizar el formateo y emisión de constantes flotantes.
* **Acciones:**
  1. Crear `static void emit_float_const(emitter_t *e, const char *key);` que realice el `strdup`, reemplazo de exponente `'s' -> 'e'`, emisión con `write_line` y liberación de memoria.
  2. Reemplazar el código duplicado en `read_symbol` y `materialize`.

---

### Fase 4: Eliminación del Flag `planning` (Doble Pasada) con `open_memstream`
* **Objetivo:** Transformar la generación de cuerpos de función a un flujo lineal de pasada única.
* **Acciones:**
  1. Usar `open_memstream` dentro de `write_body` para capturar las instrucciones en un buffer dinámico durante una única pasada de `emit_range`.
  2. Registrar auxiliares en `symbol_table` y `local_types` de manera directa en `new_local`.
  3. Tras la pasada de traducción, emitir la firma con los tipos y cantidades finales de temporales, escribir el prólogo, volcar el buffer de instrucciones con `fwrite`, escribir el epílogo y cerrar la función.
  4. Eliminar `plan_body` y el flag booleano `e->planning`.

---

### Fase 5: Modularización de Funciones Extensas
* **Objetivo:** Dividir bloques con alta complejidad ciclomática en unidades cohesivas.
* **Acciones:**
  1. Extraer `write_body_prologue` y `write_body_epilogue` de `write_body`.
  2. Modularizar `binary` extrayendo:
     * `emit_float_add_checked`
     * `emit_int16_overflow_check`
     * `emit_int16_sign_extend`
  3. Reemplazar el bucle `for (helper = 0; helper < 3; helper++)` en `write_object_helpers` por `write_field_helper(e, name, index_by_symbol_id, emit_type_kind)` invocado declarativamente para `"position"`, `"field"` y `"field_type"`.

---

### Fase 6: Segregación del Estado (`wasm_module_t` y `body_emitter_t`)
* **Objetivo:** Separar las responsabilidades del módulo global y de la emisión de cuerpos particulares.
* **Acciones:**
  1. Definir `wasm_module_t` para el estado global (archivo de salida, layout, cuerpos).
  2. Definir `body_emitter_t` para el estado transitorio del recorrido RPN (pila de operandos, pila de llamadas, temporales, cuerpo actual).
  3. Adaptar las firmas para pasar el contexto adecuado según la responsabilidad.

---

## 4. Matriz de Validación de Calidad

| Criterio | Métrica de Aceptación |
| :--- | :--- |
| **Equivalencia Semántica** | `diff` nulo contra salidas `.wat` de baseline en los 336 escenarios. |
| **Legibilidad** | Máximo 1 sentencia ejecutable por línea en C. |
| **Constantes** | 0 números mágicos sin nombre en la lógica de emisión. |
| **Acoplamiento** | Eliminación completa de la bandera `planning` y de `plan_body`. |
| **Documentación e Inventario** | `docs/FUNCTIONS.md` 100% actualizado con nombres y líneas exactas (`verify_function_docs.py` PASSED). |
| **Integridad del Conjunto de Pruebas** | `./tests/run_all.sh` finaliza con código 0. |
