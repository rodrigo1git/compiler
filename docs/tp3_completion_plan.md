# Plan de finalización de TP3

> Implementation update: the confirmed rules are implemented; see [tp3_implementation.md](tp3_implementation.md) for the current behavior. Neither group member belongs to the 2011 study plan, so Simple Reduction does not apply. The original plan and decisions below are retained as design history.

Este documento es un plan: no declara implementadas las etapas siguientes.
Parte del código actual, que ya genera polaca dinámica, maneja ámbitos y
conserva contextos de llamadas. Incorpora las seis decisiones confirmadas en
[tp3_decisions.md](tp3_decisions.md).

## Fundamento y alcance

Se distingue entre tres fuentes de decisiones:

- **Requisitos de cátedra:** TP2, TP3, 08.3 Generación y 08.4 Name Mangling.
- **Reglas confirmadas por el usuario:** completan los puntos particulares
  que los documentos no detallaban. No se atribuyen como citas de las filminas.
- **Estrategias de implementación:** concretan esas reglas conservando el
  comportamiento exigido. Las estructuras C no son requisitos de la cátedra.

Material disponible en `/home/rodrigo/facultad/compi`:

| Material | Requisito utilizado |
| --- | --- |
| TP3 pp.1–3; 08.4 diapositiva 11 | Polaca dinámica, referencias estables, tipo, uso, ámbitos, declaración previa y chequeos de nombres. |
| TP3 pp.4–5; 08.3 diapositiva 66 | Información de funciones/métodos, copia de parámetros, retorno primitivo por defecto; compatibilidad numérica e incorporación de conversiones implícitas en TP4 para LR → PI. |
| TP2 p.4; TP3 p.7 | Orden obligatorio y copia de argumentos en el orden indicado, manteniendo su posición formal; generación del tema 18. |
| TP3 p.6 | FROM: comparación estricta, selección de incremento/decremento según límites y actualización después del cuerpo. |
| TP3 p.8; 08.3 diapositivas 31–33 | Valores de compilación, sustitución de usos, contextos determinables y atributos semánticos de valor. |
| TP3 p.9 | Verificación de clases de igual código y requisito de código coincidente para asignar clases distintas. |
| TP2 p.7; TP3 pp.10–11 | Acceso posicional desde cero, existencia/visibilidad y búsqueda de miembros heredados con error por homónimos. |
| TP2 p.1; TP3 p.5 y decisión 6 | Cabeceras con retorno primitivo y valor por defecto del tipo devuelto; exclusión confirmada de retornos de objetos. |
| TP3 p.12 | Polaca legible y numerada, diagnósticos con línea, TS y trazas optativas. |

Los objetos continúan admitidos como variables, atributos y parámetros. Se
excluye su uso como resultado de funciones y métodos. La generación de assembler
y el chequeo general de compatibilidad numérica siguen siendo TP4.

## Etapa 1. Contrato semántico, retornos y orden de llamadas

Cambios en `grammar.y`, `scope` y `codegen`:

1. Comprobar que toda cabecera de función o método declare retorno INTEGER o
   SINGLEF. Un nombre de clase como retorno debe producir un diagnóstico de
   lenguaje no admitido, incluso sin generar polaca; no uno de implementación
   pendiente. Mantener los tipos de clase en las declaraciones y parámetros.
2. Rechazar valores objeto en RET conforme a esa exclusión. La comprobación de
   categoría objeto/escalar no se convertirá en un chequeador de compatibilidad
   INTEGER/SINGLEF: ese chequeo numérico sigue en TP4.
3. Mantener las listas obligatorias y las comprobaciones de cantidades. Cambiar
   los diagnósticos pendientes por errores específicos de posición repetida,
   fuera de rango o constante que no represente una posición INTEGER. La lista
   completa debe contener exactamente una vez cada posición 1..N.
4. Conservar FRAME/ARG/CALL y la asociación argumento-formal al reordenar. No
   sustituir los contextos independientes por una copia sobre símbolos globales.
5. Separar validaciones semánticas de la habilitación de impresión/generación,
   para que una regla del lenguaje no dependa accidentalmente de `--rpn`.

Aceptación: órdenes válidos producen 10 y 8 en el ejemplo de TP3; los órdenes
inválidos reciben diagnósticos definitivos; funciones y métodos con retorno de
clase se rechazan sin alterar los usos permitidos de objetos.

## Etapa 2. Esquema efectivo de clases y diamantes

Cambios principales en `symbol_table` y `scope`:

1. Mantener separados los atributos declarados por la clase y una lista de
   atributos efectivos, cuyos elementos refieren a las declaraciones originales.
2. Construir esa lista con heredados primero y propios después. Para concretar
   el recorrido entre bases se propone conservar el orden textual de `extends`
   y recorrer cada base con el mismo criterio. Esta es una convención explícita
   de representación; no está prescrita literalmente por las filminas y no
   concede prioridad de resolución a la primera base.
3. Deduplicar por identidad de declaración, nunca por nombre ni tipo. El mismo
   atributo alcanzado por dos caminos ocupa una sola posición. Dos atributos
   distintos con el mismo nombre no se fusionan.
4. Aplicar la misma identidad a la búsqueda de métodos. Una declaración propia
   se resuelve antes de buscar bases. Dos declaraciones heredadas distintas
   homónimas producen el error del tema 32; dos caminos al mismo símbolo no.
5. Llevar el estado del recorrido para detectar ciclos y evitar búsquedas o
   esquemas recursivos sin fin. Restaurar el contexto ante errores.
6. Conservar la resolución de nombres en su punto de aparición: un cierre o
   recorrido posterior no debe volver visible una declaración escrita después
   del uso. Esto también rige para llamadas y cuerpos que se especializarán.

Aceptación: cadenas de herencia y diamantes tienen una numeración determinista;
la base compartida no duplica declaraciones; los homónimos distintos siguen
siendo errores al acceder conforme al tema 32.

## Etapa 3. Compatibilidad y copia de objetos

Depende del esquema efectivo de la etapa 2.

1. Comparar clases que comparten código cuando sus declaraciones estén
   completas, aunque el programa no llegue a asignar objetos de esas clases.
2. Exigir igual cantidad de atributos efectivos e igualdad del tipo en cada
   posición. No comparar nombres de atributos ni incluir firmas de métodos,
   según la decisión confirmada.
3. Identificar tipos de clase por sus referencias semánticas calificadas; dos
   nombres textuales iguales en ámbitos distintos no son automáticamente el
   mismo tipo. No introducir conversiones para hacer iguales tipos distintos.
4. Mantener el requisito de código coincidente para copias entre clases
   distintas. Diagnosticar estructura incompatible en vez de devolver pendiente.
5. Definir la copia intermedia por posiciones correspondientes de los atributos
   efectivos. Puede conservarse una operación abstracta de copia de objeto si
   referencia expresamente ambos esquemas y su correspondencia. No se impondrán
   offsets de memoria ni instrucciones assembler en TP3.

Aceptación: clases de igual código y secuencia de tipos compatible se aceptan
aunque cambien nombres o métodos; cambiar cantidad, tipo u orden produce error;
la copia deja estados independientes y no alias entre objetos.

## Etapa 4. Accesos posicionales heredados e integración de métodos

1. Reemplazar el rechazo actual de todo acceso con herencia por consultas al
   esquema efectivo. Un índice constante identifica una declaración concreta;
   verificar su existencia y visibilidad, incluido el tema 32.
2. Mantener los índices variables como referencias. Registrar el esquema para
   que TP4 pueda seleccionar el atributo y efectuar los controles que necesiten
   ejecución. No inventar una posición constante para un valor desconocido.
3. Preservar receptor y declaración original al ejecutar un método heredado.
   El acceso implícito al atributo debe llegar al estado del objeto receptor y
   usar el mismo esquema que su acceso posicional.
4. Unificar la comprobación de miembros para que la numeración no permita
   ignorar una ambigüedad de nombres del tema 32.

Aceptación: la lectura posicional de un atributo heredado y su lectura dentro
de un método observan el mismo valor; escrituras sobre otro objeto no lo alteran.

## Etapa 5. Preparación para evaluar comptime

El parser actual emite cuerpos de control durante las reducciones. No alcanza
con modificar una variable global al reconocer cada asignación: se procesarían
ambas ramas y cada ciclo una sola vez.

1. Conservar temporalmente cuerpos de función, selección y ciclo como fragmentos
   con límites de control y referencias ya resueltas. Reutilizar los fragmentos
   de expresiones existentes. La forma interna concreta es una estrategia de
   traducción dirigida por la sintaxis, no una representación final nueva:
   la salida del grupo sigue siendo polaca dinámica.
2. Dar a las expresiones información de valor conocido/desconocido y de efectos
   sobre el entorno de compilación. No dar valor cero por defecto a una variable
   comptime aún no asignada.
3. Separar ese entorno por ámbito y por activación. La plantilla de una función
   no debe mutar el entorno del principal al ser declarada.
4. Conservar líneas y guardas de recursos para descartar construcciones fallidas
   sin publicar cambios de entorno ni fragmentos parcialmente válidos.

Aceptación: la infraestructura conserva la polaca existente cuando no aparece
comptime, incluidos los efectos laterales del tema 18 y las llamadas anidadas.

## Etapa 6. Evaluación y sustitución comptime

1. Evaluar asignaciones y expresiones con valores demostrablemente conocidos;
   registrar el valor por símbolo y reemplazar cada lectura comptime por una
   constante de TS del tipo correspondiente, según TP3 p.8.
2. Mantener la semántica de las operaciones del lenguaje, sus representaciones
   INTEGER/SINGLEF y TOI; no usar el rango ilimitado de una herramienta auxiliar
   como si fuera el del lenguaje. Esta evaluación necesaria para comptime no
   implica incorporar el chequeador numérico general de TP4.
3. Rechazar un uso sin valor determinable o una asignación dependiente de datos
   desconocidos. No asumir que una variable común es conocida por estar declarada;
   todo conocimiento sobre ella debe estar probado y mantenerse válido frente
   a escrituras y llamadas.
4. En condiciones conocidas, evaluar únicamente el camino ejecutado para los
   efectos comptime. El análisis sintáctico y de nombres continúa cubriendo el
   código fuente completo.
5. En condiciones desconocidas, permitir lecturas de valores comptime ya
   conocidos y código ordinario, pero diagnosticar cada modificación comptime
   bajo ese control en la línea de asignación, incluso si ambas ramas escribirían
   lo mismo. No diferir el error hasta un uso posterior.
6. Para ciclos con efectos comptime, procesar las iteraciones cuyo control pueda
   determinarse y materializar la polaca equivalente de los efectos ejecutables.
   En FROM, actualizar al final el valor actual del control, respetando posibles
   cambios del cuerpo; no deducir siempre las iteraciones sólo de los límites.
7. Rechazar las modificaciones comptime bajo control de ciclo desconocido. Los
   ciclos ordinarios conservan BF/BI. Controlar falta de progreso del evaluador
   para evitar que la compilación quede colgada, sin inventar reglas como BY>0.
8. Eliminar almacenamiento y asignaciones de ejecución para las variables
   exclusivamente comptime; mantener sus valores y atributos en la TS para
   inspección. Conservar todo efecto ejecutable que acompañe su evaluación.

Aceptación: `x:=3$i; x:=x+1$i; z:=x;` sustituye 4$i; una condición conocida no
ejecuta ambas ramas; una modificación bajo condición desconocida falla en su
asignación; ciclos conocidos producen los valores de cada iteración.

## Etapa 7. Comptime en funciones, métodos y orden de argumentos

TP3 p.8 incluye parámetros comptime; dejarlos siempre como unsupported no
completaría el tema asignado.

1. Retener las funciones y métodos como plantillas con sus referencias léxicas.
   Crear las especializaciones necesarias a partir de valores conocidos de sus
   parámetros comptime y del entorno comptime externo que utilicen.
2. Identificar cada especialización separadamente del nombre fuente y conservar
   referencias al símbolo original. Esto permite diferentes valores en distintas
   llamadas sin cambiar la búsqueda de nombres ni modificar otra activación.
3. Procesar argumentos en el orden confirmado del tema 19, incluyendo sus
   asignaciones del tema 18. El valor disponible al llegar al argumento determina
   el enlace del parámetro comptime. No evaluar todos antes de aplicar el orden.
4. Rechazar argumentos no determinables destinados a parámetros comptime; un
   argumento comptime conocido puede copiarse a un parámetro común, como muestra
   TP3. No convertir automáticamente todos los parámetros comunes en comptime.
5. Distinguir el enlace y los valores locales de cada especialización de las
   modificaciones a símbolos comptime externos. No ejecutar en compilación los
   POUT ni los efectos ordinarios de una función; deben permanecer en su polaca.
6. Propagar los efectos sobre el entorno de compilación a través de llamadas,
   para impedir que una llamada dentro de control desconocido oculte una
   modificación de un símbolo comptime externo. Mantener la línea de la
   asignación responsable y, si hace falta, el contexto de invocación.
7. Conservar los retornos primitivos por defecto y los contextos independientes
   ya implementados. Distinguir recursión ordinaria de una evaluación/especialización
   de compilación que no pueda progresar.

Aceptación: dos llamadas con valores comptime diferentes generan efectos
ejecutables correspondientes a cada valor sin contaminarse; los argumentos
desconocidos se rechazan y las llamadas ordinarias siguen pasando las pruebas
de copia, anidamiento, recursión y resultados 10/8 del tema 19.

## Etapa 8. Integración, pruebas y entrega

1. Ampliar las expectativas existentes, sin reemplazar la suite por pruebas
   temporales. Cambiar los antiguos casos Pending clarification por positivos
   o negativos según las seis decisiones confirmadas.
2. Añadir familias de pruebas de retornos prohibidos, permutaciones, estructuras
   compatibles/incompatibles, herencia múltiple, diamantes, accesos estáticos y
   dinámicos, comptime local/global, ramas, ciclos y parámetros.
3. Verificar recuperación y restauración de ámbitos, activaciones, entornos y
   recursos. Un error no debe ocultar errores independientes ni dejar salida
   parcial presentada como programa válido.
4. Extender el evaluador de pruebas para los esquemas efectivos y las
   especializaciones. Mantener expectativas calculadas desde los ejemplos y
   reglas, no sólo snapshots que reproduzcan lo que imprime la implementación.
5. Construir con -Wall -Wextra -Werror y cero conflictos de Bison; ejecutar las
   regresiones y ASan/UBSan/detección de fugas. Registrar resultados reales sin
   prometer de antemano una cantidad de pruebas aprobadas.
6. Mostrar polaca numerada y nombres de símbolos, diagnósticos con línea y TS.
   Conservar las trazas como opción y no publicar operaciones provisionales
   pendientes de sustitución comptime en una compilación exitosa.
7. Actualizar README, decisiones, matriz de requisitos e informe/PDF final.
   Marcar separadamente las pruebas TP4 aún no aplicables.

## Criterio de cierre

TP3 se considerará completo cuando todas las construcciones del alcance
confirmado generen polaca o sus errores semánticos específicos, los ejemplos
de la cátedra tengan pruebas reproducibles y no queden marcadores pendientes
para las seis decisiones. El chequeo numérico general y assembler siguen
siendo TP4, no excusas para omitir comprobaciones de nombres, clases o comptime.

La única condición externa conservada es Reducción Simple: TP3 p.11 la exige
si algún integrante pertenece al plan 2011. Si aplica, el cierre incluye esa
optimización y sus pruebas a partir del material donde la cátedra la define.
Sin esa confirmación no se certificará cumplido ese requisito condicional.
