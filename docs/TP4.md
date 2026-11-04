# Generación WebAssembly del grupo 24

El compilador conserva su análisis léxico, parser Bison, ámbitos y polaca inversa.
El modo `--wasm` agrega compatibilidad de tipos y genera WebAssembly mediante
variables auxiliares. El runtime de JavaScript carga el módulo, muestra sus salidas
y comunica sus errores; los cálculos y controles se ejecutan en WebAssembly.

## Temas y consigna

Los temas registrados por el grupo son 3, 7, 10, 14, 15, 18, 19, 22, 24, 29,
32 y 34: INTEGER int16, SINGLEF f32, cadenas multilínea, FROM, comentarios `//`,
asignaciones internas, orden de argumentos, comptime, clases con código, acceso
posicional, herencia con ambigüedad y TOI. La asignación original de esos temas no
está incluida en el repositorio. TP3 p.1 confirma polaca inversa para grupo 24;
TP4 p.2 asigna cdgi. La fecha escrita en TP4 es 13/11/2026.

TP4 pide generar destino desde la polaca mediante auxiliares, ensamblarlo y
ejecutarlo, conservar IR y TS, traducir conversiones y agregar los controles
asignados. WebAssembly es una alternativa expresamente admitida en TP4 p.1.

## Construcción y ejecución

Requisitos: Bash, GCC, Bison >=3.6, Python 3 y Node >=18. WABT se fija en 1.0.39.

```sh
./build.sh
npm ci --prefix runtime
./compile_wasm.sh tests/tp3_templates.txt -o /tmp/templates.wat --run
```

El script genera `/tmp/templates.wat` y `/tmp/templates.wasm`. Una ruta de salida
que coincida con la entrada se rechaza. Para separar las etapas:

```sh
./compiler --wasm -o /tmp/program.wat tests/tp3_comptime_copies.txt
node runtime/assemble.mjs /tmp/program.wat /tmp/program.wasm
node runtime/run.mjs /tmp/program.wasm
```

Sin `--wasm`, los modos históricos siguen realizando TP3. `--analyze` evita
imprimir la polaca; combinado con `--wasm` conserva los chequeos y la generación
del archivo. WAT se publica mediante un archivo temporal y renombrado al completar
la generación. Un fallo no reemplaza una salida anterior válida; siempre hay que
comprobar el estado del proceso para identificar el resultado actual.

Para navegador, servir `runtime/` con HTTP y abrir `index.html`:

```sh
python3 -m http.server 8000 --directory runtime
```

El selector carga un `.wasm` local. El navegador y Node comparten `host.mjs`.
El runner Node es el que utiliza la suite principal. La prueba opcional
`python3 tests/verify_tp4_browser.py` abre Chromium headless y ejercita el selector
del navegador. Chromium necesita permisos de IPC que el aislamiento del agente
puede bloquear; ejecutar esa prueba en un entorno normal.

## Recorrido y tipos

1. `main` interpreta opciones, inicializa y llama a `yyparse`.
2. Las acciones de `grammar.y` construyen cuerpos fuente en `codegen.c`.
3. En modo completo, `types_check_sources` valida todos esos cuerpos, incluidos
   los no invocados, las ramas constantes y el código posterior a RET.
4. `specialize_program` resuelve comptime y conserva sus instancias y plantillas.
   Antes de eliminar un brazo, ciclo vacío o cola posterior a RET,
   `types_check_discarded` revisa una copia de ese rango con el entorno actual.
   Resuelve índices de parámetros y locales comptime y comprueba también los
   contextos de asignación, argumento, operación, TOI y retorno. Recorre ambos
   brazos de controles anidados sin aplicar efectos al entorno verdadero.
   Las copias de objetos conservan los campos en el instante de su lectura.
   Para resolver valores que provienen de llamadas reutiliza el especializador
   sobre ese entorno aislado; no agrega esas llamadas al cuerpo que se ejecutará.
5. `layout_init` calcula símbolos, clases, campos, cadenas y marcos. Detecta ciclos
   de contención por valor, incluso en clases sin instancias.
6. Los cuerpos ejecutables se vuelven a comprobar: aquí se resuelven obligaciones
   cuyo tipo dependía de un índice comptime pendiente en la plantilla.
7. `wasm_write` planifica reservas y auxiliares y emite el módulo estructurado.
8. WABT valida y convierte el archivo; Node o navegador ejecuta el módulo real.

No hay conversiones implícitas. Operaciones binarias y asignaciones requieren
compatibilidad; argumentos se comprueban contra sus formales y RET contra el
retorno declarado. TOI recibe SINGLEF. Las clases se copian por estructura/código
conservando las reglas existentes de TP3.

Para un índice ordinario variable, se comprueban todas las alternativas del uso.
Una suma INTEGER sobre una clase con candidatos INTEGER y SINGLEF es inválida.
POUT directo admite selección heterogénea numérica: se conserva el valor y su
categoría y se llama al import correspondiente. Una plantilla que usa un índice
comptime pendiente conserva esa obligación hasta sus especializaciones; no se
rechaza por desconocer todavía el argumento. El índice se evalúa una vez y se
controla su rango antes del acceso. Escribir mediante índice ordinario en una
selección que pueda alcanzar un campo comptime se rechaza estáticamente.

## Auxiliares y bifurcaciones

La pila del traductor contiene referencias a auxiliares y destinos. Cada lectura
mutable conserva su valor antes de efectos posteriores. Cada cálculo genera un
auxiliar explícito, declarado como local Wasm y registrado en TS como AUXILIARY.
DUP reutiliza el valor, DROP conserva los efectos ya emitidos.

Para `a := b + 2$i*c`, el recorrido guarda las lecturas de b y c, genera un
auxiliar para el producto y otro para la suma, y escribe el resultado en a.
Los nombres usan IDs deterministas y namespaces separados: `fn_ID` para cuerpos,
`tmp_ID_N` para auxiliares y etiquetas locales por posición de control.
Los símbolos se enumeran por nombre interno ordenado; las referencias del backend
son punteros C y nunca se recuperan analizando la salida textual de la polaca.

Los destinos de polaca empiezan en 1; count+1 es una salida válida. Los pasos de
control conservan inicio de condición, clase IF/FROM y variable FROM. Al
especializar, se reconstruyen esas ubicaciones para el cuerpo resultante.

Pseudocódigo IF: traducir condición; abrir if con su auxiliar; traducir rama
verdadera; traducir ELSE si existe; cerrar if. Pseudocódigo FROM: abrir block de
salida y loop; traducir condición; salir si es falsa; traducir cuerpo y actualización;
saltar a loop; cerrar ambos. Se usan etiquetas estructuradas, sin despachador de PC.
El comentario `RPN N, source line L` permite localizar cada operación emitida.

## Almacenamiento, llamadas y objetos

La memoria lineal tiene 16 MiB y se exporta. Cada marco comienza con el enlace
léxico y el receptor (dos direcciones i32); siguen las variables, parámetros y
reservas de objetos temporales. Los escalares ocupan cuatro bytes; INTEGER sigue
normalizándose a int16 y SINGLEF usa f32. El tamaño de cada marco se conoce antes
de emitir cuerpos; `$allocate` controla la reserva y limpia la memoria nueva.
Los límites son restricciones de esta implementación, no controles adicionales
asignados por la cátedra.

FRAME reserva un marco pendiente, ARG copia cuando corresponde según la lista de
orden y CALL entra al cuerpo. Las llamadas anidadas tienen marcos pendientes
independientes. El enlace léxico apunta al marco de la declaración exterior,
aunque el llamador sea otra función. RET guarda el resultado y salta a un epílogo
único; el llamador recupera su pila de marcos. La caída al final devuelve cero.

Cada objeto contiene un identificador de clase y campos efectivos. Los campos
objeto se almacenan por valor de forma contigua. Los métodos reciben la dirección
del receptor; el helper `$field` traduce identidad de atributo y clase real a su
ubicación. Así un método heredado funciona aunque su base no sea la primera,
sin clonar cuerpos por cada esquema ni suponer un desplazamiento único.

Los slots de campos comptime se reservan dentro de las copias materializadas:
transportan constantes capturadas y permiten copiarlas hacia campos ordinarios
compatibles. No habilitan escrituras comptime ordinarias en ejecución. Las
asignaciones comptime siguen resolviéndose y eliminándose durante compilación.
Una lectura variable que puede elegir campos comptime conserva una captura del
receptor, visible como `capture:` en IR y como OBJECT_VALUE en TS.

Las lecturas de objetos realizan snapshots independientes. MATERIALIZE aplica
las capturas, incluidos hijos. `$copy_object` copia campos por posición y tipo
entre esquemas admitidos; no copia indiscriminadamente bytes entre clases
distintas. Esto conserva originales, parámetros, campos hermanos y orden de
argumentos. El índice de un destino variable se guarda antes de evaluar su RHS; el paso
CHECK_INDEX verifica sus límites antes de ejecutar efectos del lado derecho.

## Política numérica y controles cdgi

Las condiciones se emiten dentro del módulo. El import `runtimeFail(code,line)`
presenta el mensaje y lanza una excepción. Después de cada llamada de fallo se
emite unreachable por si otro host retornara.

| Control | Condición y ejecución |
| --- | --- |
| c | Suma SINGLEF ampliada a f64, comprobación del límite superior de magnitud de TP1, redondeo a f32 y nueva comprobación. |
| d | Producto de operandos int16 extendidos a i32; comprobar −32768..32767 antes de normalizar. |
| g | TOI comprueba primero rango/finitud y luego ausencia de fracción. 3.0 convierte; 3.9 y −3.9 fallan. |
| i | Indicador por función fuente compartido por sus especializaciones; comprobar y activar al entrar, limpiar en epílogo. Incluye reentrada indirecta y por otro receptor. |

`f(f(x))` es válido si la llamada interior termina antes de entrar en la exterior.
Una operación ordinaria que deba provocar un control permanece en el ejecutable.
Si el resultado es necesario para un valor comptime, se informa durante compilación.
En TP4, una llamada a una función fuente que ya se está especializando apunta
al cuerpo activo y conserva resultado desconocido. Su guardia EI termina antes
que se ejecute ese cuerpo otra vez, aunque el argumento comptime cambie como
p-1 o p+1. No se crean infinitas instancias para una reentrada ordinaria. Los
argumentos conservan sus efectos previos a la llamada. Esta regla no se activa
en TP3, que conserva su evaluación y límites anteriores.
Los límites de profundidad/expansión del especializador protegen al compilador;
no sustituyen la guardia de recursión del ejecutable.

Las sumas/restas INTEGER se normalizan modularmente a int16; 32767+1 produce
−32768. La división entera trunca hacia cero y también normaliza. La división
entera por cero conserva el trap del destino; no se atribuye al grupo el control a.
Las operaciones flotantes no controladas conservan IEEE 754, incluidos subnormales
y valores especiales. Una constante comptime materializada debe además ser
representable conforme al rango de literales. Estas políticas se aplican en
`numeric.c` y en la emisión; no son reglas de overflow adicionales citadas del TP.

## Verificación y entrega

```sh
./tests/run_all.sh --report /tmp/tp4-report.json
```

TP3 conserva sus suites y su evaluador. Se corrigió la aplicación recursiva de
Object fields y se agregaron comprobaciones de cuerpos simbólicos. TP4 convierte
y ejecuta el Wasm para comparar resultados independientes, errores, estado de
salida y ausencia de POUT posteriores. Los casos de TP3 con tipos diferidos,
recursión o truncamiento tienen expectativas explícitas para la etapa final.
El reporte incluye hashes del binario y fuentes, versiones y escenarios.

La validación requiere revisar también ejemplos manuales de salida y WAT.
Un recuento de pruebas no demuestra la ausencia de cualquier fallo posible.
Las evidencias concretas de esta ejecución se registran en VALIDATION.md.
