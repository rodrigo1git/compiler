# Requisitos generales y particulares del grupo 24

Análisis realizado el 4 de noviembre de 2026 sobre las consignas originales de
TP1, TP2, TP3 y TP4, la documentación local y el código de este repositorio.
El alcance es el trabajo completo, hasta la generación y ejecución del código
destino. Este documento identifica obligaciones y puntos abiertos; no implementa
el TP4 ni certifica el cumplimiento integral del compilador.

## Fuentes y criterio de lectura

Las páginas indicadas son las páginas físicas de los PDF:

- [TP1: léxico y entrega inicial](/home/rodrigo/facultad/compi/Tp01-2026.pdf).
- [TP2: sintaxis y requisitos del parser](/home/rodrigo/facultad/compi/Tp02-2026.pdf).
- [TP3: representación intermedia y semántica](/home/rodrigo/facultad/compi/Tp03-2026.pdf).
- [TP4: código destino, controles y entrega final](/home/rodrigo/facultad/compi/Tp04-2026.pdf).
- [Generación de código: Polaca y conversiones](</home/rodrigo/facultad/compi/08.3.Generación de Código LR-AS LR-PI LR-T.pdf>).
- [Name mangling](</home/rodrigo/facultad/compi/08.4.Name Mangling.pdf>).
- [Conversiones en Pentium](/home/rodrigo/facultad/compi/11.2.Assembler_Pentium_Conversiones.pdf).
- [WebAssembly](/home/rodrigo/facultad/compi/WebAssembly2026.pdf).

La identificación del grupo y sus doce temas consta en [README.md](README.md) y
[el informe del grupo](docs/report.md). No se encontró la planilla original del
Aula Virtual con la asignación de esos temas. La asignación de Polaca al grupo 24
y la de los controles `cdgi` se verifican directamente en TP3 p.1 y TP4 p.2.

Se distinguen los requisitos expresos de cátedra, las decisiones ya registradas
del grupo y las propuestas de implementación. Los ejemplos genéricos deben
adaptarse a nuestros tipos, sufijos, ciclo y orden obligatorio de argumentos.
Una omisión o errata en un ejemplo no modifica automáticamente la regla general.

## Asignación del grupo

| Tema | Característica | Fuente principal |
| --- | --- | --- |
| 3 | INTEGER con signo de 16 bits y sufijo `$i` | TP1 p.3; TP2 p.9 |
| 7 | SINGLEF de 32 bits con exponente `s` | TP1 p.3 |
| 10 | Cadenas multilínea entre comillas dobles | TP1 p.3 |
| 14 | Ciclo `from ... to ... by ... repeat` | TP2 pp.3–4; TP3 p.6 |
| 15 | Comentarios de línea con `//` | TP1 p.3 |
| 18 | Asignaciones de identificadores o constantes dentro de expresiones | TP2 p.4; TP3 p.7 |
| 19 | Orden obligatorio de evaluación y copia de argumentos | TP2 p.4; TP3 p.7 |
| 22 | Variables y parámetros `comptime` | TP2 p.5; TP3 p.8 |
| 24 | Clases con código y asignación por estructura | TP2 pp.5–6; TP3 p.9 |
| 29 | Acceso posicional a atributos | TP2 p.7; TP3 p.10 |
| 32 | Herencia múltiple con error por ambigüedad | TP2 pp.7–8; TP3 pp.10–11 |
| 34 | Conversión explícita de SINGLEF a INTEGER mediante `toi` | TP2 p.8; TP3 p.4; TP4 p.3 |

El grupo genera **Polaca Inversa almacenada dinámicamente**. TP4 asigna los
controles **c, d, g e i**. La fecha indicada en TP4 es **13/11/2026**; TP3 señala
que se entrega junto con TP4. Esto refleja los PDF disponibles, sin verificar
eventuales cambios comunicados por el docente.

## Requisitos generales por etapa

### TP1: análisis léxico

1. Reconocer identificadores de hasta 22 caracteres: primera posición letra,
   resto letras, dígitos o `_`; las letras de identificadores son minúsculas.
   Truncar los que excedan el límite e informar warning, no rechazarlos por longitud.
2. Reconocer palabras reservadas sin distinguir mayúsculas de minúsculas.
   Esta excepción no habilita mayúsculas en identificadores comunes.
3. Reconocer `+`, `-`, `*`, `/`, `:=`, `=`, `>=`, `<=`, `>`, `<`, `==`, `!=`,
   paréntesis, coma, punto y coma y los símbolos de los temas asignados.
   Para el grupo se agregan corchetes y punto para invocaciones de métodos.
4. Omitir comentarios, blancos, tabulaciones y saltos de línea como tokens.
   Mantener información de línea para los diagnósticos.
5. Implementar el léxico mediante matriz de transiciones y matriz de acciones
   semánticas: cada transición y acción depende del estado y carácter leído.
6. Almacenar identificadores, constantes y cadenas en una tabla de símbolos
   dinámica. Una cantidad fija de buckets con nodos dinámicos no impone por sí
   misma un máximo fijo de símbolos.
7. Recibir por consola la ruta del archivo fuente elegida por el usuario.
   La interfaz gráfica no es obligatoria. Si se generan archivos de salida en
   esta etapa, la consigna indica dejarlos junto al archivo compilado.
8. Informar tokens, errores con línea y descripción, y tabla de símbolos en
   la entrega inicial. TP3 modifica la salida habitual de tokens y estructuras.

### TP2: sintaxis e integración

1. Programa con nombre, declaraciones y cuerpo ejecutable delimitado por
   `begin`/`end`; sentencias terminadas con `;`.
2. Declaraciones `<tipo> variable, variable;`, funciones anidadas, clases,
   atributos, métodos y objetos de una clase.
3. Funciones con tipo de retorno y al menos un parámetro formal tipado.
   Sus declaraciones preceden al cuerpo ejecutable. Los métodos tienen la forma
   de las funciones del ejemplo de clase, sin la palabra `function`.
4. Admitir varios `ret(expresión);` y también ausencia de retorno explícito.
   La generación debe respetar el retorno anticipado y el valor por defecto.
5. Expresiones con variables, constantes, llamadas, atributos y operaciones
   aritméticas. No admitir agrupación aritmética general con paréntesis.
   Los paréntesis de llamadas, condiciones, `pout`, `ret` y `toi` sí corresponden.
6. Selección `if (comparación) cuerpo [else cuerpo] end_if;`, con cuerpo simple
   o bloque. Las comparaciones usan expresiones y los seis comparadores.
7. Salida `pout(cadena);` y `pout(expresión);`.
8. Generar el parser con Yacc o equivalente; integrar `yylex` y devolver mediante
   `yylval` las referencias a TS de identificadores, constantes y cadenas.
9. Resolver en el parser el signo de constantes negativas y volver a controlar
   los límites que dependen de ese signo.
10. Continuar el análisis después de errores recuperables y eliminar **todos**
    los conflictos shift/reduce y reduce/reduce.

TP2 exige mostrar tokens y estructuras sintácticas con línea. Desde TP3 estas
trazas deben quedar deshabilitadas por defecto, salvo devolución docente que
exija mantenerlas. Una opción de depuración es compatible con esa separación.

### TP3: Polaca, símbolos y semántica

1. Generar IR para todas las sentencias ejecutables: operaciones, asignaciones,
   selección, FROM, salida, conversión, llamadas y retornos.
2. Almacenar la IR en una estructura dinámica: es requisito explícito de aprobación.
3. Conservar referencias a TS en los operandos, aunque al imprimir se muestren
   los nombres y literales correspondientes. Numerar los pasos y destinos.
4. Generar el cuerpo de cada función y método declarado, con límites claros
   respecto del principal y las demás funciones, incluso si no se invoca.
5. Registrar tipo y uso de símbolos; tipo de constantes desde el léxico;
   tipo de identificadores desde la declaración; retorno y lista de parámetros
   con nombres y tipos; metadatos de clases y de los temas particulares.
6. Asociar símbolos a ámbitos mediante name mangling, por ejemplo `x:main:f`.
   La visibilidad comienza en la declaración y alcanza el ámbito y sus ámbitos
   contenidos. Resolver la declaración visible más cercana.
7. Permitir homónimos en ámbitos distintos; rechazar redeclaraciones en el mismo
   ámbito, también entre categorías como variable, función y clase.
8. Diagnosticar símbolos inexistentes, no visibles o usados con una categoría
   incorrecta: una función no es una variable asignable ni una variable una clase.
9. Verificar cantidad de argumentos y realizar pasaje por copia. El orden del
   tema 19 modifica cuándo se evalúa y copia cada argumento, conservando el formal
   que corresponde a su posición original.
10. Proveer retorno por defecto del tipo declarado. La elección de cero es una
    decisión del proyecto; TP3 exige el tipo y no fija un literal único.
11. Mostrar IR legible, tabla de símbolos y todos los diagnósticos de compilación
    con línea y descripción, sin las trazas habituales de TP1/TP2.

**Distribución del chequeo de tipos:** TP3 p.4 y TP4 p.3 indican que para Polaca
Inversa el chequeo general de compatibilidad se hace durante la traducción a
código destino. Esto incluye operaciones, asignaciones, comparaciones y los
controles de tipos asociados a llamadas y retornos. Los chequeos de nombres,
códigos de clase y determinabilidad de `comptime` siguen siendo necesarios en TP3.
Evaluar `comptime` también requiere valores representables del tipo declarado.

### TP4: generación y ejecución

1. Traducir la Polaca a código destino usando **variables auxiliares**.
2. Elegir uno de los destinos admitidos: Pentium de 32 bits o WebAssembly.
   La existencia de material de ambos destinos no constituye una elección del grupo.
3. Si se elige Pentium, usar registros enteros y el coprocesador 80X87 para
   flotantes, según TP4 p.1. Si se elige WebAssembly, mantener INTEGER de 16 bits
   aunque se lo represente mediante un valor `i32`.
4. Traducir todas las operaciones y cuerpos admitidos, incluidos parámetros,
   retornos, objetos, atributos, herencia, `toi` y `pout`; resolver auxiliares,
   etiquetas, datos y soporte de ejecución del destino.
5. Comprobar compatibilidad de tipos al traducir y emitir diagnósticos que
   identifiquen la combinación incompatible. Implementar la conversión explícita
   en código destino; una operación `TOI` en la Polaca no completa esta etapa.
6. Insertar los cuatro controles asignados. Ante cada error de ejecución,
   emitir un mensaje descriptivo y terminar el programa generado.
7. Entregar código que pueda ensamblarse o procesarse y ejecutarse. Interpretar
   la IR en el verificador de Python no sustituye esta comprobación.
8. Mostrar diagnósticos de compilación, IR y TS, y producir el archivo de código
   destino. Documentar cómo construir y ejecutar ese resultado y sus dependencias.

## Semántica particular y casos que deben probarse

### Temas 3 y 7: tipos numéricos

INTEGER ocupa 16 bits con signo: `-32768..32767`. Las constantes usan `$i`.
Debe aceptarse `-32768$i` y rechazarse `32768$i`; aceptar provisionalmente la
magnitud en el léxico no autoriza su uso positivo en el programa.

SINGLEF es de 32 bits. Se exige punto y parte decimal; la parte entera y el
exponente son opcionales. El exponente comienza con `s` y admite signo:
`1.0`, `.6`, `-1.2`, `3.0s-5`, `1.2s+10`. `12.`, `12s3` y un exponente sin
dígitos no cumplen el formato. TP1 fija el rango abierto
`1.17549435s-38 < abs(x) < 3.40282347s+38`, más `0.0`.

Probar límites válidos e inválidos para ambos tipos, signos, sufijos incompletos,
formatos de exponente y redondeo al materializar valores SINGLEF. No sustituir
el rango de INTEGER por el del entero nativo de C o WebAssembly, ni el de
SINGLEF por el rango ampliado de un registro 80X87.

### Temas 10 y 15: cadenas y comentarios

Las cadenas se delimitan por `"` y pueden abarcar varias líneas. En TS se
almacenan sin saltos de línea. Los comentarios empiezan con `//` y llegan al
fin de línea; el proyecto también admite su terminación al EOF.

Probar cadenas multilínea, vacías y sin cierre, comentarios, división `/`, EOF
sin salto final y CRLF. `//` dentro de una cadena es contenido de la cadena.
CHAIN es la categoría del literal; no se exige un tipo de variable String.

### Tema 14: FROM

Forma: `from i := CTE1 to CTE2 by CTE3 repeat cuerpo;`.
La variable y las tres constantes deben ser INTEGER para este grupo.

- Inicializar la variable con CTE1 antes de comparar.
- Si CTE1 <= CTE2: comparar `i < CTE2` y, después del cuerpo, sumar CTE3.
- Si CTE1 > CTE2: comparar `i > CTE2` y, después del cuerpo, restar CTE3.
- Límites iguales ejecutan cero iteraciones. El límite final queda excluido.
- La actualización utiliza el valor de control que queda después del cuerpo,
  incluidos cambios causados por llamadas.

No se prescribe que BY sea positivo o distinto de cero. Un ciclo ordinario
potencialmente infinito no es un error de compilación por ese solo motivo.
Probar ascenso `1..9`, descenso `10,8,6,4,2`, límites iguales, sobrepaso del
límite, cambios del control y anidamientos IF/FROM. El proyecto conserva dos
`;` para cuerpo simple: uno de la sentencia y otro del ciclo.

### Tema 18: asignaciones en expresiones

El operador interno es `=` y el valor asignado es un identificador o constante,
no una expresión arbitraria como en el tema 17. La asignación produce el valor
que participa en la expresión exterior y conserva su efecto sobre el destino.
Por ejemplo, `a := b=2$i + c;` asigna 2 a b y luego calcula la suma para a.

La IR debe conservar el valor además de realizar la escritura; `DUP` es la
solución elegida por el proyecto. Las reglas de tipos son las de la asignación
normal. Probar cadenas, precedencia, efectos laterales y argumentos reordenados.
El proyecto distingue la sentencia ordinaria `id := expresión` de estos usos de
`=`; para atributos admite `:=` y `=` por las decisiones registradas de reentrega.

### Tema 19: evaluación y copia ordenadas

Cada llamada debe incluir `f(argumentos)[orden]`, también una llamada de un
solo argumento y las llamadas a métodos. Para el grupo, las posiciones se
escriben como constantes INTEGER, por ejemplo `[1$i,3$i,2$i]`.

La cantidad de argumentos, formales y posiciones debe coincidir. El grupo
definió que el orden sea una permutación de `1..N`, sin repeticiones ni omisiones.
Los atributos de objetos empiezan en cero; las posiciones del orden empiezan en uno.

Evaluar y copiar cada argumento cuando lo indica el orden. No evaluar todos
primero para luego reordenar sus valores. El tercer argumento sigue ligado al
tercer formal aunque se evalúe antes que el segundo. Los ejemplos de TP3 deben
producir 10 y 8 con los órdenes respectivos. Probar también una operación no
conmutativa, llamadas anidadas, objetos por copia y efectos sobre el receptor.

### Tema 22: COMPTIME

Registrar el valor en cada punto de uso y sustituir la referencia por la
constante correspondiente. No basta con almacenar el último valor global.
La variable existe durante compilación, sin requerir almacenamiento ordinario
en ejecución. TP3 p.8 también muestra parámetros formales `comptime`.

Se debe poder determinar el valor: rechazar usos sin valor conocido, asignaciones
que dependan de valores desconocidos y argumentos desconocidos para un formal
`comptime`. No presumir cero en una variable aún no inicializada.

Las ramas conocidas y los ciclos determinables deben actualizar el estado del
camino efectivamente ejecutado. El grupo acordó rechazar modificaciones bajo
control desconocido en la asignación, incluso si ambas ramas escribirían lo mismo.
Esto también alcanza efectos producidos mediante llamadas.

Al combinarlo con funciones y objetos, separar activaciones y receptores,
capturar valores cuando se evalúa cada argumento y preservar los efectos de
ejecución como `pout`. La especialización, `MATERIALIZE` y sus metadatos son
estrategias del proyecto. Sus límites de recursos no son restricciones nuevas
sobre los ciclos ordinarios del lenguaje.

### Tema 24: clases con código

Las clases llevan nombre y código. Las que comparten código deben tener
estructura compatible, aunque no se efectúe una asignación entre ellas.
La asignación entre clases distintas exige código coincidente.

El grupo definió compatibilidad por cantidad, tipos y orden de los atributos
efectivos; los nombres de atributos y los métodos no participan. Los tipos de
clase conservan su identidad de ámbito. Comparar estructuras por nombres de
campos o por la cantidad de bytes del destino no satisface esa decisión.

La copia debe relacionar posiciones correspondientes y conservar independencia
entre objetos. Probar código igual con estructura igual o diferente, código
distinto con estructura igual, atributos heredados y copias con campos `comptime`.

### Temas 29 y 32: posiciones y herencia

El acceso es `objeto[constante]` u `objeto[variable]`, desde índice cero, con
verificación de existencia y visibilidad. Los métodos se invocan con punto,
por ejemplo `objeto.metodo(x)[1$i]`.

La herencia se declara con `extends base, otra;` dentro de la clase. Buscar
primero en la clase del objeto y después en las bases. Si aparecen declaraciones
heredadas distintas con el mismo nombre, informar error; elegir la primera base
corresponde al tema 30 y no a nuestro tema.

El grupo definió atributos heredados antes que propios y deduplicación por
identidad de declaración en diamantes. El código recorre bases en orden textual
para formar la lista efectiva; eso no concede prioridad al resolver homónimos.
Un método heredado debe operar sobre los atributos del receptor correcto.

Probar posiciones constantes y variables, índices inexistentes y negativos,
visibilidad por orden de declaración, herencia en cadena y múltiple, diamantes,
homónimos distintos y coincidencia entre acceso implícito de método y posicional.
El backend deberá resolver los accesos variables; el contrato de tipos cuando
pueden seleccionar atributos heterogéneos requiere definición adicional.

### Tema 34: TOI y compatibilidad

`toi(expresión)` convierte SINGLEF a INTEGER y puede usarse como operando en
cualquier contexto de expresión permitido. Su resultado es INTEGER. Para el
grupo no corresponde insertar conversiones implícitas ni agregar `tosf` por
la sola presencia de INTEGER y SINGLEF.

Rechazar combinaciones numéricas incompatibles en operaciones, asignaciones,
comparaciones, parámetros y retornos. Ejemplos de tipos correctos:
`a := toi(x+y);` y `a := toi(x)+b;`, con a y b INTEGER, x e y SINGLEF.
La aceptación de tipos no garantiza que una conversión concreta supere el
control de pérdida de información de TP4.

TP3 p.4 contiene un ejemplo aparentemente válido con `toul(x)+z`, indicando z
flotante: ni la función corresponde a nuestro tema 3 ni esa suma cumple la
compatibilidad descrita. Debe tratarse como inconsistencia del ejemplo.

## Los cuatro controles de ejecución del grupo

| Control | Operación alcanzada | Comportamiento exigido y prueba discriminante |
| --- | --- | --- |
| c | Suma SINGLEF | Detectar resultado fuera del rango del tipo, mostrar error y terminar. Probar suma válida y desbordes positivos y negativos usando operandos individualmente válidos. |
| d | Producto INTEGER | Detectar resultado fuera de `-32768..32767`, mostrar error y terminar. `200$i * 200$i` y `-32768$i * -1$i` exceden el rango aunque quepan en un entero de 32 bits. |
| g | Conversión de flotante a entero | Detectar pérdida de información, mostrar error y terminar. Probar conversión exacta dentro de rango, fracción no nula y valores fuera del rango INTEGER. |
| i | Invocación recursiva | Detectar que una función se invoque a sí misma durante la ejecución, mostrar error y terminar. Probar recursión directa y llamadas repetidas que ya retornaron. |

Para c, no basta verificar desborde del formato extendido de 80X87 si el valor
ya no cabe en SINGLEF. Para d, no basta el flag de overflow de una multiplicación
de 32 bits si el resultado está fuera de los 16 bits del lenguaje.

Para g, la lectura literal de pérdida de información incluye perder una fracción:
`toi(3.0)` puede producir 3; truncar silenciosamente `toi(3.9)` no cumple esa
lectura. Las filminas generales describen instrucciones de conversión y
truncamiento, pero no derogan el control particular. Conviene confirmar con el
docente si utiliza una definición más restringida de pérdida de información.
No alcanza un trap genérico del destino si falta el mensaje exigido.

Para i, un indicador de activación por función, restablecido en todos sus retornos,
es una estrategia posible. También detectaría reingreso indirecto `f -> g -> f`;
la consigna no desarrolla ese caso, por lo que debe declararse esa interpretación.
Una segunda llamada después de finalizar la primera debe funcionar. Si se
especializan funciones, conviene conservar la identidad de la función fuente
para que cambiar de instancia no oculte un reingreso.

No se asignan al grupo los controles a, b, e, f ni h. En particular, división por
cero no integra su lista particular de TP4. El soporte de división sigue siendo
parte del lenguaje. Los chequeos necesarios para implementar acceso posicional
no deben confundirse con esa lista de cuatro controles aritméticos y de llamadas.

## Decisiones existentes y puntos abiertos

En [tp3_decisions.md](docs/tp3_decisions.md) y el informe actual se registran estas
decisiones previas del grupo:

- Rechazar escrituras `comptime` bajo control desconocido.
- Comparar clases por tipos y orden de atributos; excluir nombres y métodos.
- Numerar atributos heredados antes que propios.
- Identificar declaraciones compartidas en diamantes, sin duplicarlas por camino.
- Exigir una permutación completa del orden de argumentos.
- Admitir retornos INTEGER y SINGLEF; excluir retornos de objetos.
- Excluir Reducción Simple porque ningún integrante pertenece al plan 2011.

La prohibición de retornos de objetos es una decisión registrada, no una
prohibición textual encontrada en las consignas. El retorno por defecto cero,
el recorrido textual de bases y la organización concreta de la IR son elecciones
de implementación que deben explicarse como tales.

Puntos a cerrar para TP4:

1. **Destino y entorno de ejecución:** Pentium o WebAssembly, herramientas,
   bibliotecas y forma de entregar/ejecutar el resultado.
2. **Conversión con pérdida:** documentar la interpretación del control g y
   alinear el evaluador y los ejemplos con ella.
3. **Recursión:** documentar el alcance del control i para reingreso indirecto,
   métodos y especializaciones, conservando llamadas secuenciales válidas.
4. **Índices variables en clases heterogéneas:** definir el tipo del acceso cuando
   diferentes posiciones tienen tipos diferentes, la resolución en destino y el
   tratamiento de posiciones inválidas. TP2 admite índices variables pero no
   precisa esta combinación. No corresponde prohibirlos todos por comodidad.
5. **Cuerpos simbólicos:** mostrar el código retenido de plantillas `comptime`
   sin inventar valores para generar una instancia ejecutable inexistente.
6. **Casos sintácticos límite:** la gramática actual exige declaraciones y cuerpo
   principal no vacíos, además de `;` tras el `end` final. Los textos y ejemplos
   no fijan uniformemente todos esos límites; son convenciones a mantener
   explícitas y contrastar con la devolución docente.

## Pruebas y documentación de entrega

Las consignas piden el código fuente completo, ejecutable, componentes necesarios,
pruebas e informe. Si se usa GitHub, TP1 p.2 exige repositorio privado; no se
comprobó su visibilidad remota en este análisis.

El informe debe mantener número de grupo, integrantes, correos y temas asignados;
introducción; decisiones justificadas; y, acumulativamente:

- TP1: autómata, matriz de transiciones, mecanismo de ambas matrices, acciones
  semánticas y tratamiento de errores léxicos.
- TP2: gramática y no terminales, problemas y soluciones, recuperación y
  eliminación de conflictos, catálogo de errores y conclusiones.
- TP3/TP4: almacenamiento dinámico de IR, uso de `$$`/`$n`, pseudocódigo de
  bifurcaciones, nuevos errores, generación de código destino, operaciones,
  etiquetas, cambios de etapas previas y solución de cada tema en cada etapa.
- Uso de IA: herramientas, prompts disponibles, partes generadas y justificación.
  Los integrantes deben poder explicar y defender el compilador completo.
- Conclusiones e instrucciones reproducibles para construir y ejecutar.

Las pruebas deben cubrir límites léxicos, todas las estructuras válidas y errores
recuperables; declaración, redeclaración, categoría y alcance; IF/FROM anidados;
asignaciones internas; evaluación y copia ordenadas; `comptime` válido e inválido;
clases de igual código con estructuras compatibles e incompatibles; asignaciones
de objetos; atributos, herencia y ambigüedad; y tipos/conversiones válidos e inválidos.

Para cada control c/d/g/i incluir un caso válido, uno que active el error y una
salida posterior que permita comprobar que la ejecución terminó. Las pruebas
finales deben ejecutar el código destino, además de examinar su texto o la IR.
Los escenarios de TP3 que aceptan mezclas de tipos por diferimiento deben tener
su expectativa específica de rechazo en TP4. Los que ejecutan recursión o
truncamiento con fracción deben reflejar los controles del grupo al validar TP4.

Los temas alternativos de otros grupos, como WHILE, enumeraciones, AUTO,
import/export, amistad y conversiones implícitas, no son requisitos propios.
Los ejemplos genéricos que los usan necesitan adaptación al lenguaje del grupo.

## Evidencia sobre este repositorio

Se inspeccionaron el punto de entrada, la gramática, estructuras de IR, manejo
de ámbitos, generación y especialización, junto con los informes y verificadores.
Los resultados observados en esta revisión fueron:

| Aspecto | Evidencia y alcance |
| --- | --- |
| Verificador original de TP3 | `python3 -B tests/verify_tp3.py`: 107 comprobaciones aprobadas sobre el binario existente. |
| Escenarios de cátedra | `python3 -B tests/verify_tp3_course.py --report /tmp/analisis-requerimientos-tp3-resultados.json`: 170 escenarios, 340 ejecuciones aprobadas en dos modos. |
| Conflictos de gramática | Bison ejecutado sobre `src/grammar.y`, con ambos tipos de conflicto tratados como error: salida exitosa sin conflictos. Archivos generados en `/tmp`. |
| Plantilla sin invocar | Programa temporal con `f(comptime integer p)` aceptado: TS muestra la plantilla, pero su cuerpo no aparece en la salida. Confirma el pendiente de TP3 pp.1,5,12 señalado por el informe. |
| Código destino y controles TP4 | No se identificó un backend Pentium/WebAssembly, archivo destino ni los cuatro controles en ejecución. La documentación y el punto de entrada mantienen esa etapa pendiente. |
| Cobertura de objetos anidados | El informe advierte que las últimas correcciones de capturas no tienen cobertura permanente equivalente y que el evaluador no interpreta independientemente todos sus metadatos anidados. |
| Documentación y entrega | `.gitignore` excluye `docs/`: los informes locales no están incluidos por un commit normal. Verificar su inclusión expresa en el paquete de entrega. Los PDF y JSON históricos no demuestran por sí solos el estado actual. |

Las 447 comprobaciones son evidencia favorable para TP3; no prueban el cumplimiento
de TP4. Esta revisión no recompiló el ejecutable, no volvió a ejecutar sanitizers
y no cambió el compilador ni las expectativas de las pruebas. El único archivo
añadido al proyecto es este análisis.

Las prioridades que surgen de los requisitos son cerrar la presentación de
plantillas, definir el contrato de los puntos abiertos del backend y completar
la traducción tipada con auxiliares y los controles cdgi; después, validar los
programas generados y actualizar el informe y el paquete de entrega.
