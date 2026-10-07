# Compilador TP2 - Fase 5: Documentación Final

Este documento presenta la arquitectura final del Analizador Léxico y el Analizador Sintáctico, así como las decisiones de diseño, manejo de errores y estructuras de datos implementadas para cumplir con la especificación del Trabajo Práctico 2.

## 1. Analizador Léxico (Autómata Finito Determinista)

El Analizador Léxico se implementó mediante una matriz de transiciones (Tabla de Transiciones) que modela un Autómata Finito Determinista (AFD).

### Diagrama de Transición de Estados

A continuación se detalla el diagrama de estados del autómata, generado a partir de `transition_table.c`.

```mermaid
stateDiagram-v2
    direction LR
    classDef error fill:#f99,stroke:#333,stroke-width:2px;
    classDef final fill:#9f9,stroke:#333,stroke-width:2px;
    
    state "0: START" as 0
    state "1: ID" as 1
    state "2: SLASH" as 2
    state "3: COMMENT" as 3
    state "4: EXCL / COLON" as 4
    state "5: INT_PART" as 5
    state "6: DOLLAR" as 6
    state "7: DOT" as 7
    state "8: FLOAT_DEC" as 8
    state "9: EXP_S" as 9
    state "10: EXP_SIGN" as 10
    state "11: EXP_DIGITS" as 11
    state "12: STRING" as 12
    state "13: EQ" as 13
    state "14: CMP" as 14
    
    state "F_RET (Retorno con ungetc)" as F_RET
    state "F_CONS (Retorno consumiendo)" as F_CONS
    state "E (Error Léxico)" as E
    
    class F_RET,F_CONS final
    class E error

    [*] --> 0

    0 --> 1: L, i, s
    0 --> 5: D
    0 --> 7: .
    0 --> F_CONS: +, -, *, (, ), ;, ,, [, ]
    0 --> 2: /
    0 --> 13: =
    0 --> 14: <, >
    0 --> 4: !, :
    0 --> 12: "
    0 --> 0: \n, ws

    1 --> 1: L, i, s, D, _
    1 --> F_RET: otro

    2 --> 3: /
    2 --> F_RET: otro

    3 --> 0: \n
    3 --> 3: otro

    4 --> F_CONS: = (:=, !=)
    4 --> E: otro

    5 --> 5: D
    5 --> 6: $
    5 --> 7: .
    5 --> F_RET: otro

    6 --> F_CONS: i (Sufijo entero)
    6 --> E: otro

    7 --> 8: D
    7 --> E: otro

    8 --> 8: D
    8 --> 9: s
    8 --> F_RET: otro

    9 --> 10: +, -
    9 --> 11: D
    9 --> E: otro

    10 --> 11: D
    10 --> E: otro

    11 --> 11: D
    11 --> F_RET: otro

    12 --> 12: (cualquiera excepto ")
    12 --> F_CONS: "

    13 --> F_CONS: = (==)
    13 --> F_RET: otro

    14 --> F_CONS: = (<=, >=)
    14 --> F_RET: otro
```

### Manejo de Tokens Especiales y Reglas Léxicas
- **Identificadores y Palabras Reservadas**: Todo identificador es procesado en el estado `1`. Al alcanzar un estado final, se verifica su longitud (`<= 25` caracteres). Si supera el límite, se trunca y se emite un warning. Luego se busca en la Tabla de Símbolos. Si es una palabra reservada, se devuelve su Token específico; de lo contrario, se devuelve `TOKEN_ID`.
- **Constantes Enteras**: Se valida su sufijo obligatorio `$i` (estado 6). Además, en la fase semántica posterior se aplica la validación de rango (Ghost Tokens) para diferenciar `-32768` (válido) de constantes en out-of-range reales.
- **Constantes Flotantes**: Exigen parte entera (opcional si inicia con `.`), punto decimal obligatorio y aceptan formato exponencial (`s+10`, `s-4`, etc.). La validación de underflow/overflow (rango `1.175494e-38` a `3.402823e+38` en valor absoluto o cero) se reporta mediante una acción semántica y se satura a `0.0s0` o el máximo si es necesario.
- **Strings Multilínea (Cadenas)**: Todo caracter dentro de las comillas dobles (incluido `\n`) pertenece al string. Si se detecta EOF antes de cerrar comillas, se reporta error de "Unclosed string literal".
- **Comentarios**: Los comentarios de línea inician con `//` y consumen todos los caracteres hasta encontrar `\n`, retornando al estado START.

---

## 2. Analizador Sintáctico y Resolución de Ambigüedades

El Analizador Sintáctico, construido en Yacc/Bison, procesa los tokens de abajo hacia arriba de forma *LALR(1)*. Se llevaron a cabo importantes refactorizaciones arquitectónicas para asegurar resiliencia y cumplir los requerimientos.

### 2.1 Refactorización a Recursividad a la Izquierda
Todas las producciones de listas (`id_list`, `arg_list`, `const_list`, `param_decl_list`, y secuencias de sentencias) fueron reestructuradas usando recursión por izquierda, lo que minimiza el crecimiento de la pila de Bison, optimizando el consumo de memoria para programas extensos.

### 2.2 Requisitos y Reglas Específicas
1. **Tema 18 - Asignaciones Estrictas**:
   Las expresiones complejas se modularon con la regla `assign_chain`. Se restringe el lado derecho de las asignaciones encadenadas para que solo acepten variables o constantes (`TOKEN_ID` o `constant`), bloqueando llamadas a funciones o expresiones aritméticas puras intermedios según norma del TP.
2. **Tema 24 / 30 / 32 - Clases y Herencia (Extends)**:
   Se modeló la cláusula `extends` como una sentencia interna del `class_body`. Esto permite estructuras del tipo:
   ```pascal
   class mascota c11 begin
       integer dueno;
       extends animal_base;
   end;
   ```
3. **Tema 29 - Acceso Posicional a Atributos**:
   Se crearon dos reglas diferenciadas:
   - `attr_ref` para R-values: Representa la lectura (`ID[cte]`) utilizable en expresiones matemáticas.
   - `attr_access` para L-values: Se limitó explícitamente a `attr_ref '=' expr` para representar escritura sin confundirse con el operador de igualdad (`==`) o asignación estándar (`:=`).
4. **Sentencia de Retorno (ret_stmt)**:
   Se forzó gramaticalmente que la instrucción `ret` solo pueda estar dentro de funciones (creando contextos `func_compound_stmt` gemelos), impidiendo que se utilice en el bloque del programa principal (Main).

### 2.3 Manejo de Errores y "Panic Mode" Avanzado (Evitar Cascadas)
Uno de los logros fundamentales de esta fase fue la implementación de un sistema de **Panic Mode sin propagación en cascada**:
- **Consumo Excesivo Evitado**: Se retiraron llamadas prematuras a `yyerrok` (particularmente en asignaciones simples) que hacían que el parser intentara seguir parseando en el mismo token ofensor, causando dobles errores (cascading syntax errors) para un solo punto de fallo real.
- **Puntos de Sincronización Estratégicos**: En vez de depender únicamente de una recuperación general al final de la línea (`error ';'`), se añadieron puntos de captura de error localizados:
  - En la lista de atributos y firmas de Clases (`class_def`).
  - Dentro de parentesis de IF (`error ')'`) para malformaciones lógicas.
  - Al lado derecho de asignaciones truncadas (ej. `x := ;`), permitiendo emitir mensajes precisos como `"Missing or malformed expression in assignment"`.

### 2.4 Control de Status Code (Exit Codes)
El ciclo del compilador verifica el conteo real de `global_errors` (léxicos y sintácticos).
- **Si hay 0 errores:** Retorna `exit(0)` y emite `"Parsing successful."`.
- **Si hay 1 o más errores:** Retorna `exit(1)` y emite `"Compilation failed with X errors."`.

---

## 3. Conclusión
El compilador en la Fase 5 logra 40/40 pruebas superadas en la suite integral automatizada (`run_tests.sh`), lo que demuestra una robustez total. Acepta casos correctos construyendo correctamente la TS (Tabla de Símbolos) y aborta en casos incorrectos con un diagnóstico exacto, control de cascadas sintácticas anulado y prevención garantizada contra rebasamiento de límites léxicos.
