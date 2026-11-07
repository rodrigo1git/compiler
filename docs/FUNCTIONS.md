# Recorridos para entender el compilador

Este documento describe la rama con backend WebAssembly. La documentación
descargada que menciona semantic_actions.c y clases dentro de scope.c corresponde
a una base anterior. Las funciones nuevas no requieren reescribir las matrices
léxicas ni el parser generado.

## Una asignación sencilla

`a := 2$i + 3$i;` empieza en main y yyparse. yylex lee caracteres; get_col y las
matrices eligen sa_int_const, que interna las constantes y devuelve TOKEN_CONST.
La gramática reduce factor/term/expr. codegen_value crea fragmentos y
codegen_binary los une con ADD. scope_resolve encuentra a y codegen_assignment
agrega el destino y ASSIGN. rpn.c posee esos pasos y presta sus símbolos.

types_check_sources comprueba INTEGER + INTEGER y el tipo del destino con todas
las opciones. specialize_program conserva el cálculo ordinario y
types_check_specializations comprueba las instancias. Si se solicita Wasm, layout_init
ubica a; wasm_write prepara los cuerpos. read_symbol guarda operandos en locales
auxiliares y binary emite la suma. emit_operator escribe el resultado en a.
El módulo resultante se valida con assemble.mjs y se ejecuta mediante host.mjs.

## Quién decide cada cosa

| Archivo | Entradas, decisiones y resultado |
| --- | --- |
| main.c | Opciones y archivo; ordena análisis, tipos, especialización y salida; libera recursos. |
| lexer.c | Caracteres y estado; pide acciones y devuelve token, ubicación y referencia semántica. |
| transition_table.c | Clasifica caracteres y define estados siguientes. |
| lexer_actions.c | Arma lexemas, reconoce reservadas/constantes y registra símbolos. |
| symbol_table.c | Posee símbolos y sus metadatos; sus referencias sobreviven a ampliaciones de arrays de IR. |
| diagnostics.c | Presentación, recuento, recuperación y deduplicación por origen. |
| scope.c | Declaración, categoría, nombre por ámbito y búsqueda léxica. |
| classes.c | Bases, identidad de miembros, ambigüedad y atributos efectivos. |
| grammar.y | Reglas Bison y acciones que llaman a scope/codegen; guardas y destructores. |
| rpn.c | Almacena, concatena, reubica, completa y muestra instrucciones; conserva metadatos de control. |
| codegen.c | Construye los cuerpos fuente y expone el principal; prepara copias y orden de argumentos. |
| specialize.c | Coordina la sesión y conserva la API pública, registro de fuentes, consultas e impresión. |
| specialize_effects.c | Resume escrituras/dependencias y aplica efectos a entornos reales o privados. |
| specialize_instances.c | Prepara activaciones, administra la caché y crea/libera instancias ejecutables. |
| specialize_transform.c | Recorre polaca con pila y llamadas privadas; transforma expresiones y controles. |
| specialize_probe.c | Explora requisitos por iteración en entornos privados sin publicar código ni diagnósticos. |
| comptime_values.c | Entornos conocidos/desconocidos; puente entre valores numéricos y constantes de TS. |
| comptime_objects.c | Identidades de campos anidados, capturas y copia de conocimiento por objeto. |
| numeric.c | Resultado numérico y error explícito con la política numérica única; no emite Wasm. |
| types.c | Tipos, compatibilidad y pila de tipos de todos los cuerpos; pospone obligaciones dependientes de índices comptime. |
| layout.c | IDs, tamaños de objetos, offsets y marcos; detecta contención recursiva. |
| wasm_emit.c | Planifica auxiliares/snapshots y escribe cuerpos, controles y helpers Wasm. |
| runtime/assemble.mjs | WABT: parsea WAT, resuelve nombres, valida y escribe Wasm. |
| runtime/host.mjs | Imports de salida/error y carga del módulo; no implementa los cálculos del lenguaje. |

## Una llamada con efectos

codegen_arguments_append retiene cada expresión en su posición formal original.
codegen_call_end emite FRAME, evaluación/copia ARG según la permutación y CALL.
specialize_transform conserva esa secuencia y specialize_instances elige la instancia concreta.

begin_call del backend reserva un marco pendiente y su enlace léxico. Cada ARG
es un destino dentro de ese marco. call_function llama al cuerpo, guarda el
resultado y recupera sp. Las llamadas anidadas apilan registros pendientes
distintos. El callee usa el frame recibido para parámetros/locales y el enlace
estático para nombres de funciones exteriores. La entrada comprueba EI; el
bloque return centraliza la limpieza del indicador.

## Un objeto y un método heredado

classes.c conserva cada atributo por identidad de declaración. layout_class_size
y layout_field_offset asignan el esquema físico. El header del objeto conserva
la clase real. receiver_address obtiene el receptor; symbol_address llama al
helper field cuando el símbolo es un atributo implícito. El helper busca su
offset en la clase real, incluso si la declaración pertenece a otra base.

read_symbol realiza una copia temporal del objeto. materialize aplica constantes
capturadas por comptime_capture_object y sus hijos. La asignación llama al helper
copy_object, que relaciona posiciones de origen y destino y copia por valor.

## Un índice que depende de comptime

En la plantilla `ret(object[position])`, position aún puede no tener valor.
type_of_attribute devuelve TYPE_COMPTIME_PENDING para un índice comptime pendiente.
Cuando specialize recibe el argumento, reemplaza el índice por su constante.
La validación del cuerpo concreto identifica el campo y comprueba su tipo real.
Antes de eliminar un brazo, ciclo vacío o cola posterior a RET, check_discarded
resuelve sus índices con el entorno actual y comprueba también ese código fuente.
La comprobación conserva copias temporales del entorno y de las lecturas de
objetos. Si necesita el resultado de una llamada, consulta el especializador
con sus argumentos y aplica sus efectos sólo a esa copia del entorno.
Una función no invocada mantiene su plantilla visible y la obligación pendiente.
Un índice ordinario exige compatibilidad para todos los candidatos.

TYPE_CANDIDATES representa los tipos posibles de los atributos seleccionables.
TYPE_COMPTIME_PENDING también puede propagarse a una expresión cuyo tipo depende
de esos argumentos; no representa únicamente un acceso a atributo.

## Un error de producto

`200$i * 200$i` pasa el chequeo de tipos: ambos operandos son INTEGER.
El generador emite el producto i32 y lo conserva en un auxiliar; antes de estrechar
comprueba −32768..32767. Se llama a runtimeFail(2,line) y se termina antes de
cualquier salida posterior. Si el resultado se exige para una asignación comptime,
numeric_binary registra NUMERIC_PRODUCT y assign_comptime emite el diagnóstico.

## Propiedad de memoria

TS posee nodos/textos/metadatos y se destruye al final. Los cuerpos de codegen
poseen pasos; las plantillas prestan esos cuerpos y las instancias poseen sus
copias transformadas. layout posee un array temporal de referencias/ubicaciones.
wasm_write posee planes, pilas y locales de traducción y los libera al terminar.
Los auxiliares retenidos pertenecen a TS. El archivo se cierra también cuando
la generación falla. El host se ejecuta fuera del compilador C.

## Inventario individual

Este inventario cubre **328 funciones C propias**, públicas y static, de
src/*.c y los tres ayudantes escritos en grammar.y. Excluye funciones generadas
por Bison y funciones de bibliotecas. Los enlaces apuntan al archivo y línea de
esta versión. Las fichas de infraestructura se vinculan al requisito que apoyan;
la cátedra no exige esos nombres de función ni una función por requisito.

Cada contrato indica operación, resultado y efectos relevantes. Salvo indicación
contraria, referencias a símbolos/cuerpos son prestadas; TS conserva sus símbolos,
los contextos poseen sus arrays y no se ejecuta código del usuario desde el emisor.
Las propiedades generales se describen también en «Propiedad de memoria» arriba.

| Código | Requisito |
| --- | --- |
| G01 | Reconocimiento léxico por matrices y reglas comunes de tokens. |
| G02 | TS dinámica, referencias estables y metadatos. |
| G03 | Parser, reglas sintácticas y recuperación de errores. |
| G04 | Declaraciones, categorías y alcance léxico. |
| G05 | Polaca, cuerpos, numeración y bifurcaciones. |
| G06 | Funciones, métodos, copia de argumentos y retornos. |
| G07 | Diagnósticos, línea de origen y presentación. |
| G08 | Compatibilidad de tipos y obligaciones dependientes de comptime. |
| G09 | Generación y ejecución del destino mediante auxiliares. |
| G10 | Propiedad de recursos, construcción y entrega reproducible. |
| T03 / T07 | INTEGER int16 / SINGLEF f32. |
| T10 / T15 | Cadenas multilínea / comentarios de línea. |
| T14 / T18 | FROM / asignaciones internas. |
| T19 / T22 | Orden de argumentos / evaluación comptime. |
| T24 / T29 / T32 | Clases con código / acceso posicional / herencia. |
| T34 | Conversión TOI. |
| EC / ED / EG / EI | Controles c, d, g e i del TP4. |

### classes.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [class_set_code](../src/classes.c#L8) | Copia el código escrito en la cabecera al símbolo de clase. | G02 T24 |
| [class_add_base](../src/classes.c#L17) | Recibe clase y base ya resuelta; añade la referencia a bases e invalida el esquema efectivo. No adquiere los símbolos. | G02 T32 |
| [append_distinct](../src/classes.c#L23) | Añade un símbolo a una lista sólo si esa misma referencia todavía no aparece. | T32 |
| [find_members](../src/classes.c#L30) | Busca primero un miembro propio y después sus bases; acumula candidatos por identidad y señala ciclos mediante un camino temporal. | G04 T32 |
| [class_lookup_member](../src/classes.c#L43) | Recibe clase, nombre y alcance de búsqueda; devuelve símbolo prestado y banderas separadas de ambigüedad y ciclo. Libera listas de recorrido locales. | G04 T32 |
| [class_inherited_member](../src/classes.c#L61) | Resuelve un miembro heredado implícito; caminos al mismo símbolo no son ambiguos, pero declaraciones distintas homónimas sí. | G04 T32 |
| [class_resolve_method](../src/classes.c#L70) | Busca un método en la clase y bases, deduplica identidad, valida categoría y distingue ausencia, ambigüedad y ciclo. | G04 G06 T32 |
| [class_build_layout](../src/classes.c#L85) | Construye atributos efectivos heredados primero y propios después, deduplicando por identidad; registra estados de recorrido para detectar ciclos. | T24 T29 T32 |
| [class_same_layout](../src/classes.c#L111) | Compara cantidad y tipos en posiciones efectivas, usando identidad de clase y tipo declarado; no compara nombres ni métodos. | T24 |
| [class_complete](../src/classes.c#L122) | Finaliza el esquema y compara con otras clases completas de igual código, informando estructuras incompatibles. | T24 |
| [class_attribute_at](../src/classes.c#L134) | Obtiene el atributo de una posición válida y aplica la comprobación de ambigüedad de nombres heredados. | T29 T32 |
| [class_check_copy](../src/classes.c#L158) | Compara los códigos de clases de origen/destino; devuelve admisibilidad e informa código incompatible. No copia objetos. | T24 |
| [type_in_path](../src/classes.c#L171) | Consulta si una clase pertenece al camino de contención activo; no modifica símbolos. | T22 T24 |
| [class_has_comptime_path](../src/classes.c#L204) | Recorre atributos efectivos con el camino de tipos activo; devuelve existencia de campos comptime sin reservar memoria de objetos. | T22 T24 |
| [class_has_comptime](../src/classes.c#L214) | Recibe una clase y devuelve si ella o un campo objeto contienen atributos comptime; inicia el recorrido protegido contra ciclos. | T22 T24 |
| [check_containment_path](../src/classes.c#L177) | Recorre sólo atributos objeto y detecta una clase repetida en el camino de contención por valor. Memoriza clases ya comprobadas para evitar repetir recorridos compartidos. | G08 T24 |
| [class_check_containment](../src/classes.c#L194) | Comprueba contención finita para todas las clases declaradas, incluso sin instancias. No confunde parámetros de métodos con atributos contenidos. | G08 T24 |

### codegen.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |

| [codegen_init](../src/codegen.c#L26) | Inicializa la polaca principal, el registro de funciones y el especializador. Deja el principal como cuerpo actual. | G05 T22 |
| [codegen_destroy](../src/codegen.c#L36) | Destruye especializaciones y cuerpos fuente, conservando prestados los símbolos hasta que main libere la TS. | G05 G10 |
| [codegen_print](../src/codegen.c#L50) | Delega la impresión de Polaca especializada en specialize_print. | G05 G07 |
| [codegen_finalize](../src/codegen.c#L54) | Ejecuta la especialización del programa después del parseo. | G05 T22 |
| [codegen_source_body](../src/codegen.c#L58) | Devuelve un puntero prestado al cuerpo principal fuente, antes de especializar; permite revisar todos sus tipos. | G05 G08 |
| [new_fragment](../src/codegen.c#L60) | Reserva e inicializa un fragmento de Polaca propietario de su array de pasos. | G05 |
| [emit_fragment](../src/codegen.c#L66) | Recibe un fragmento propio; lo concatena en el cuerpo actual y lo destruye. NULL no produce instrucciones. | G05 |
| [append_assignment](../src/codegen.c#L77) | Añade destino y ASSIGN al fragmento recibido, con su línea; modifica el fragmento sin emitirlo todavía. | G05 T18 |

| [codegen_arguments_append](../src/codegen.c#L99) | Conserva un fragmento de expresión como siguiente argumento, manteniendo su posición original. | G06 T19 |
| [codegen_arguments_free](../src/codegen.c#L110) | Libera todos los fragmentos de argumentos y su contenedor. | G06 G10 |
| [codegen_order_append](../src/codegen.c#L117) | Guarda la siguiente constante del orden, incluyendo una posición inválida nula para no falsear cantidades durante recuperación. | T19 |
| [codegen_order_free](../src/codegen.c#L129) | Libera el array y contenedor del orden, sin liberar sus símbolos. | T19 G10 |
| [validate_argument_count](../src/codegen.c#L135) | Consulta argumentos y formales de una llamada; informa cantidades diferentes sin cambiar el orden de evaluación. | G06 T19 |
| [codegen_call_begin](../src/codegen.c#L142) | Guarda función, receptor, argumentos y línea; verifica cantidad de reales contra formales. | G06 T19 |
| [codegen_call_free](../src/codegen.c#L150) | Libera el contexto de llamada y los fragmentos que todavía posee. | G06 G10 |
| [validate_evaluation_order](../src/codegen.c#L172) | Comprueba cantidades, posiciones, unicidad y existencia de los fragmentos de argumentos antes de emitir. Devuelve posiciones desde cero o NULL y libera reservas fallidas. | G06 T19 |
| [emit_call](../src/codegen.c#L198) | Recibe llamada y posiciones ya validadas; emite FRAME, evaluación y copia a formales en el orden explícito, y CALL. Los marcos pendientes preservan llamadas anidadas. | G05 G06 T19 |
| [codegen_call_end](../src/codegen.c#L217) | Valida orden INTEGER completo y único de 1..N; emite FRAME, fragmentos y copias ARG en ese orden, y CALL conservando asociación formal. | G06 T19 |
| [codegen_attribute](../src/codegen.c#L231) | Valida objeto, índice y esquema; conserva el atributo resuelto si el índice es constante. Un acceso inválido se representa mediante una estructura vacía. | T24 T29 T32 |
| [attribute_step](../src/codegen.c#L249) | Construye el paso de lectura, destino o chequeo usando el atributo previamente resuelto; mantiene receptor, índice y línea originales. | G05 T29 |
| [codegen_attribute_value](../src/codegen.c#L254) | Construye un paso de lectura posicional, conservando receptor e índice; referencia el campo concreto si puede identificarlo. | G05 T29 |
| [codegen_attribute_assignment](../src/codegen.c#L278) | Rechaza operandos inválidos al entrar, conserva el índice ordinario antes del RHS y emite la asignación. Consume el fragmento en éxito y recuperación. | G05 T18 T29 |
| [codegen_free](../src/codegen.c#L289) | Destruye un fragmento temporal y su contenedor; admite NULL. | G05 G10 |
| [codegen_value](../src/codegen.c#L295) | Construye un fragmento con una referencia a símbolo y su línea de origen. | G02 G05 |
| [codegen_unary](../src/codegen.c#L303) | Añade un operador unario y línea al fragmento recibido, devolviendo el mismo fragmento modificado. | G05 T34 |
| [codegen_binary](../src/codegen.c#L309) | Concatena izquierdo y derecho, consume el derecho y agrega la operación; conserva el orden de evaluación. | G05 |
| [codegen_target](../src/codegen.c#L322) | Acumula un destino de la asignación interna en la lista temporal de destinos. | T18 |
| [codegen_assignment_expression](../src/codegen.c#L333) | Recorre destinos de derecha a izquierda y emite DUP, destino y asignación, dejando el valor disponible para la expresión exterior. | G05 T18 |
| [codegen_assignment](../src/codegen.c#L349) | Comprueba códigos de copia directa y consume el RHS al añadir destino y ASSIGN; no decide todas las compatibilidades numéricas. | G05 T24 |
| [codegen_output](../src/codegen.c#L364) | Añade POUT al valor y lo incorpora al cuerpo actual, consumiendo el fragmento. | G05 G09 |

| [codegen_return](../src/codegen.c#L368) | Comprueba el contexto, consume la expresión y agrega RET. La prohibición de retornar objetos y la compatibilidad pertenecen al chequeo común de tipos. | G05 G06 G08 |
| [codegen_function_begin](../src/codegen.c#L376) | Exige retorno primitivo, crea cuerpo independiente, lo registra para especializar y cambia el contexto conservando el padre. | G05 G06 T22 |
| [codegen_function_abort](../src/codegen.c#L397) | Restaura el cuerpo de la función exterior o del principal cuando se cierra o descarta una guarda; no libera el cuerpo retenido. | G03 G05 |
| [codegen_function_end](../src/codegen.c#L408) | Añade RET de cero del tipo declarado para caída al final y restaura el cuerpo exterior. | G05 G06 |
| [append_branch](../src/codegen.c#L419) | Reserva un destino y emite BF o BI con línea, devolviendo la dirección a completar. | G05 |
| [codegen_if_begin](../src/codegen.c#L426) | Emite condición y BF con destino pendiente, registrando dónde comienza la condición. | G05 |
| [codegen_if_else](../src/codegen.c#L436) | Añade BI para saltar el ELSE y completa la salida falsa hacia el inicio de esa rama. | G05 |
| [codegen_if_end](../src/codegen.c#L443) | Completa el salto pendiente con el primer paso posterior a la selección. | G05 |
| [constant_value](../src/codegen.c#L447) | Convierte temporalmente el texto de una constante para comparar los límites FROM; no valida los tipos de su cabecera. | T14 |
| [codegen_for_begin](../src/codegen.c#L458) | Inicializa el control, elige comparación estricta y actualización por los límites y prepara el salto de salida. | G05 T14 |
| [codegen_for_end](../src/codegen.c#L480) | Emite actualización sobre el control actual, salto a la condición y completa la salida del ciclo. | G05 T14 |
| [append_operator](../src/codegen.c#L72) | Agrega un operador con su línea fuente en un solo paso; evita modificar por separado el último elemento del fragmento. | G05 G07 |
| [evaluation_position](../src/codegen.c#L157) | Valida tipo INTEGER y rango de una posición de evaluación. Devuelve posición desde uno o cero ante constante recuperada/errónea, conservando los diagnósticos. | T19 |
| [save_attribute_index](../src/codegen.c#L261) | Conserva el índice ordinario en un temporal del cuerpo actual y emite CHECK_INDEX antes de los efectos de la expresión derecha. Devuelve el acceso actualizado. | G05 T18 T29 |

### comptime_objects.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |

| [comptime_object_location](../src/comptime_objects.c#L23) | Devuelve una identidad estable para el par objeto padre/campo; crea una ubicación lógica si todavía no existe. | T22 T24 |
| [comptime_object_root](../src/comptime_objects.c#L38) | Sube las ubicaciones de campos hasta el objeto raíz para relacionar escrituras y dependencias. | T22 T24 |
| [type_in_path](../src/comptime_objects.c#L52) | Indica si un tipo ya pertenece al camino activo de un recorrido recursivo. | T22 T24 |
| [comptime_object_field](../src/comptime_objects.c#L58) | Obtiene el valor de un campo desde una captura o entorno y conserva la identidad/captura de un campo objeto anidado. | T22 T24 |
| [capture_object](../src/comptime_objects.c#L80) | Crea una captura inmutable de los campos comptime, incluyendo objetos hijos; diagnostica valores desconocidos o una captura recursiva no finita. | G06 T22 T24 |
| [comptime_capture_object](../src/comptime_objects.c#L124) | Inicia la captura recursiva de campos comptime del valor recibido. Devuelve símbolo de captura poseído por TS y usa el contador compartido de identidades. | G02 T19 T22 T24 |
| [copy_object_fields](../src/comptime_objects.c#L129) | Copia conocimiento por posiciones efectivas y campos anidados; verifica determinabilidad y permisos de escrituras comptime, distinguiendo copias de argumentos. | G06 T22 T24 |
| [comptime_copy_object_fields](../src/comptime_objects.c#L166) | Recibe contextos origen/destino, objeto y tipo; inicia la copia de conocimiento por posición, diferenciando argumentos y control desconocido. | G06 T22 T24 |
| [comptime_objects_init](../src/comptime_objects.c#L171) | Inicializa el registro de identidades lógicas de campos; no reserva objetos del ejecutable. | T22 T24 |
| [comptime_objects_destroy](../src/comptime_objects.c#L175) | Libera las ubicaciones lógicas y sus nombres una vez que dejaron de usarse por entornos/cuerpos. | G10 T22 |
| [comptime_resolve_object](../src/comptime_objects.c#L47) | Resuelve un atributo objeto mediante la ubicación (receptor concreto, declaración); conserva variables y parámetros y permite receptor implícito. No cambia los símbolos de la IR. | T22 T24 |

### comptime_values.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |



| [comptime_values_equal](../src/comptime_values.c#L8) | Compara estados de propagación y caché: distingue +0 y −0 SINGLEF, mantiene separados infinitos de distinto signo y no unifica NaN. No implementa el operador == del lenguaje. | T03 T07 T22 |
| [comptime_get](../src/comptime_values.c#L15) | Busca la asociación de símbolo y objeto en el entorno; devuelve valor desconocido si no está registrada. | T22 |
| [comptime_read_symbol](../src/comptime_values.c#L23) | Lee una constante o consulta un símbolo en el entorno; los atributos se buscan junto con el receptor del método y las variables sin receptor. Devuelve valor desconocido si no hay asociación. | G06 G08 T22 T29 |
| [comptime_set](../src/comptime_values.c#L29) | Actualiza o agrega una asociación símbolo/objeto/valor en el entorno. | T22 |
| [comptime_copy_environment](../src/comptime_values.c#L41) | Copia el array de asociaciones del entorno; las identidades de símbolos y capturas siguen prestadas. | T22 |
| [comptime_forget_runtime](../src/comptime_values.c#L51) | Vuelve desconocidas las asociaciones ordinarias, conservando las declaradas comptime. | T22 |
| [comptime_join_environments](../src/comptime_values.c#L57) | Combina caminos conservando sólo los valores numéricos conocidos iguales en ambos; el resto pasa a desconocido. | T22 |
| [comptime_make_constant](../src/comptime_values.c#L74) | Comprueba representabilidad, escribe el literal del tipo adecuado e interna la constante; reutiliza su referencia si ya está disponible. | G02 T03 T07 T22 |
| [comptime_read_constant](../src/comptime_values.c#L102) | Interpreta un símbolo INTEGER/SINGLEF como valor conocido y redondea a float cuando corresponde. | T03 T07 T22 |
| [comptime_binary](../src/comptime_values.c#L117) | Evalúa operandos conocidos del mismo tipo mediante numeric_binary, con una única política numérica; devuelve conocimiento, categoría y error, sin emitir código. | T03 T07 T22 EC ED |
| [comptime_to_integer](../src/comptime_values.c#L129) | Convierte un valor conocido mediante numeric_integer; conserva errores de rango o pérdida de fracción y elimina su referencia a constante original. | T22 T34 EG |

### diagnostics.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [set_semantic_origin](../src/diagnostics.c#L29) | Cambia el origen cuerpo/paso del diagnóstico y devuelve el anterior para restaurarlo después de un recorrido anidado. | G07 T22 |
| [clear_semantic_origins](../src/diagnostics.c#L35) | Libera el registro de diagnósticos semánticos emitidos y reinicia el origen activo. | G07 |
| [set_trace_enabled](../src/diagnostics.c#L45) | Activa o desactiva la salida de trazas sin modificar los controles del lenguaje. | G07 |
| [trace_printf](../src/diagnostics.c#L49) | Imprime con argumentos variables únicamente si las trazas están habilitadas. | G07 |
| [format_message](../src/diagnostics.c#L57) | Recibe formato y va_list, calcula tamaño y devuelve texto reservado. El llamador libera el resultado; falla fatalmente si no puede reservarlo. | G07 |
| [write_diagnostic](../src/diagnostics.c#L71) | Recibe categoría opcional, línea y texto; imprime el diagnóstico en stderr sin modificar el contador. | G07 |
| [register_semantic_error](../src/diagnostics.c#L77) | Consulta origen/línea/mensaje; devuelve si corresponde emitirlo y retiene una copia para evitar repeticiones del mismo origen. | G07 T22 |
| [report_semantic_error](../src/diagnostics.c#L96) | Informa y cuenta el error; si hay origen, evita repetir el mismo mensaje para el mismo cuerpo, paso y línea. | G04 G07 T22 |
| [flush_syntax_error](../src/diagnostics.c#L108) | Imprime un diagnóstico sintáctico pendiente, ya contabilizado, y desactiva su impresión futura. | G03 G07 |
| [finish_error_recovery](../src/diagnostics.c#L116) | Emite lo pendiente y limpia el estado de recuperación léxica/sintáctica. | G03 G07 |
| [report_lexical_error](../src/diagnostics.c#L121) | Imprime y cuenta un error léxico con línea y descripción. | G01 G07 |
| [report_parser_error](../src/diagnostics.c#L131) | Cierra recuperación pendiente, cuenta el error fatal del parser e imprime mensaje y línea sin categoría adicional. | G03 G07 |
| [begin_lexical_recovery](../src/diagnostics.c#L137) | Cierra un pendiente anterior y marca el nuevo episodio como originado en el léxico para evitar duplicación automática. | G01 G03 |
| [begin_syntax_error](../src/diagnostics.c#L144) | Registra línea, token inesperado y contexto; cuenta el error al detectarlo aunque no se alcance una producción recuperadora. | G03 G07 |
| [report_syntax_error](../src/diagnostics.c#L154) | Cuenta e imprime un error independiente detectado directamente por una producción diagnóstica. | G03 G07 |
| [report_recovered_syntax_error](../src/diagnostics.c#L160) | Sustituye la explicación del error pendiente por un mensaje específico, sin volver a contarlo. | G03 G07 |
| [report_recovered_syntax_error_with_context](../src/diagnostics.c#L167) | Añade el contexto pendiente al mensaje de recuperación y delega su emisión. | G03 G07 |
| [syntax_error_unexpected](../src/diagnostics.c#L177) | Devuelve el token inesperado registrado para especializar un diagnóstico del parser. | G03 G07 |
| [finish_statement](../src/diagnostics.c#L181) | Cierra la recuperación cuando no hay un error exterior pendiente ni un episodio léxico por resolver. | G03 |
| [is_lexical_recovery](../src/diagnostics.c#L187) | Indica si la recuperación actual se originó en el léxico. | G01 G03 |

### grammar.y

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [yyerror](../src/grammar.y#L527) | Callback de Bison: entrega mensaje y línea actual a report_parser_error. No implementa recuperación gramatical por sí mismo. | G03 G07 |
| [yyreport_syntax_error](../src/grammar.y#L533) | Consulta el contexto de Bison y tokens esperados, arma una explicación legible y registra un error pendiente que las producciones pueden especializar. | G03 G07 |
| [friendly_symbol_name](../src/grammar.y#L595) | Traduce nombres internos de tokens a palabras útiles para el diagnóstico, como identifier o integer. | G03 G07 |

### layout.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [layout_source](../src/layout.c#L6) | Normaliza una instancia a su declaración fuente; devuelve símbolo prestado y no calcula memoria. | G04 G06 |
| [layout_parent](../src/layout.c#L10) | Obtiene la función exterior léxica de la declaración, atravesando ámbitos de clase; devuelve referencia prestada o NULL para global. | G04 G06 |
| [entry](../src/layout.c#L17) | Busca un símbolo por identidad en el plan y devuelve su entrada prestada o NULL; no modifica el plan. | G02 G09 |
| [layout_id](../src/layout.c#L23) | Consulta la entrada y devuelve su ID determinista o cero si no existe. | G02 G09 |
| [layout_offset](../src/layout.c#L27) | Consulta la ubicación de una variable o cadena en el plan; devuelve desplazamiento/dirección registrada, sin emitir instrucciones. | G09 |
| [layout_class_size](../src/layout.c#L31) | Devuelve el tamaño calculado de una clase en bytes; se usa para reservas y copias de objetos. | G09 T24 |
| [layout_field_offset](../src/layout.c#L35) | Acumula tamaños hasta una posición efectiva y devuelve desplazamiento del campo, contando el header de clase. | G09 T29 T32 |
| [layout_frame_size](../src/layout.c#L43) | Normaliza la función fuente y calcula el extremo de sus variables/formales; incluye enlace léxico y receptor. | G06 G09 |
| [measure_class](../src/layout.c#L55) | Calcula y retiene tamaño por valor de cada clase; marca estados nombrados de visita e informa contención recursiva o límite de tamaño. | G09 T24 |
| [compare_entries](../src/layout.c#L76) | Compara nombres de entradas para ordenarlas de forma determinista con qsort; no cambia símbolos. | G02 G09 |
| [layout_init](../src/layout.c#L80) | Crea el plan propio, ordena símbolos y calcula IDs, clases, variables y cadenas. Devuelve éxito sólo sin errores de layout. | G02 G09 |
| [layout_destroy](../src/layout.c#L116) | Libera el array propio de entradas y reinicia el plan; conserva símbolos prestados de TS. | G10 |

### lexer.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [read_input_char](../src/lexer.c#L19) | Lee un carácter y produce una sola nueva línea artificial al EOF para completar tokens con anticipación; informa si el carácter es sintético. | G01 |
| [update_line_number](../src/lexer.c#L34) | Actualiza la línea real y devuelve si avanzó; cuenta CRLF una sola vez e ignora el delimitador artificial. | G01 G07 |
| [is_recovery_delimiter](../src/lexer.c#L53) | Indica si un carácter permite detener la recuperación léxica sin consumir indefinidamente otras construcciones. | G01 G03 |
| [is_valid_token_boundary](../src/lexer.c#L58) | Reconoce puntuación que conviene preservar como posible comienzo o cierre de un token después de un error. | G01 G03 |
| [push_back_char](../src/lexer.c#L62) | Devuelve un carácter al archivo y corrige el avance de línea producido al leerlo; no devuelve EOF. | G01 |
| [recover_from_lexical_error](../src/lexer.c#L73) | Consume hasta un delimitador de recuperación, preserva ese delimitador cuando corresponde y vacía el lexema inválido. | G01 G03 G07 |
| [set_token_location](../src/lexer.c#L86) | Carga la ubicación del token en yylloc para que Bison y las acciones conserven sus líneas de origen. | G03 G07 |
| [log_token](../src/lexer.c#L93) | Muestra token, lexema y línea mediante la traza opcional; no genera otro token. | G07 |
| [lexer_reset_input](../src/lexer.c#L98) | Reinicia posición, tratamiento de EOF y búfer para comenzar el archivo desde la primera línea. | G01 |
| [yylex](../src/lexer.c#L106) | Ejecuta ambas matrices, aplica anticipación y recuperación, y devuelve un token, YYerror o EOF; las acciones cargan yylval. | G01 G02 G03 |

### lexer_actions.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [ensure_lexeme_capacity](../src/lexer_actions.c#L15) | Reserva o amplía el búfer dinámico para el tamaño solicitado; una falla de memoria termina con diagnóstico interno. | G01 G02 |
| [append_to_lexeme](../src/lexer_actions.c#L37) | Añade un carácter y el terminador nulo, aumentando capacidad cuando hace falta. | G01 |
| [reset_lexeme_buffer](../src/lexer_actions.c#L43) | Deja el lexema vacío, conservando la reserva reutilizable. | G01 |
| [free_lexeme_buffer](../src/lexer_actions.c#L49) | Libera el búfer y reinicia puntero, capacidad y longitud. | G10 |
| [check_reserved_words](../src/lexer_actions.c#L79) | Devuelve el token reservado para una palabra ya normalizada a minúsculas, o −1 si no es reservada. | G01 G03 |
| [sa_init](../src/lexer_actions.c#L102) | Comienza un lexema con el carácter actual y devuelve la indicación interna de continuar. | G01 |
| [sa_append](../src/lexer_actions.c#L108) | Agrega el carácter excepto CR/LF; permite que el texto de cadenas multilínea quede sin saltos. | G01 T10 |
| [sa_token_consume](../src/lexer_actions.c#L115) | Devuelve directamente el token del carácter actual, consumiéndolo. | G01 |
| [sa_token_buffered](../src/lexer_actions.c#L121) | Devuelve el token del carácter ya guardado; la transición permite devolver el carácter de anticipación. | G01 |
| [sa_ignore](../src/lexer_actions.c#L129) | No altera el lexema ni devuelve token; se usa para blancos y comentarios. | G01 T15 |
| [sa_identifier](../src/lexer_actions.c#L133) | Distingue reservadas sin importar mayúsculas; rechaza mayúsculas en identificadores, trunca a 22 con warning e interna el identificador válido. | G01 G02 |
| [sa_int_const](../src/lexer_actions.c#L164) | Completa y valida $i y la magnitud; registra INTEGER y marca 32768 provisional para resolver su signo en el parser. | G02 T03 |
| [sa_float_const](../src/lexer_actions.c#L183) | Convierte una copia del texto reemplazando s por e, valida formato y rango SINGLEF y registra la constante sin alterar el lexema fuente. | G02 T07 |
| [sa_init_chain](../src/lexer_actions.c#L209) | Vacía el búfer al abrir una cadena para no almacenar su comilla inicial como contenido. | T10 |
| [sa_chain](../src/lexer_actions.c#L214) | Registra el contenido de una cadena cerrada como CHAIN y devuelve su token. | G02 T10 |
| [sa_multi_char_op](../src/lexer_actions.c#L219) | Completa y reconoce :=, >=, <=, == o !=; una combinación no admitida produce error léxico. | G01 |
| [sa_dot_or_float_error](../src/lexer_actions.c#L230) | Distingue un punto aislado de un intento de flotante sin parte decimal; devuelve punto o error. | G01 T07 |
| [sa_float_decimal_error](../src/lexer_actions.c#L237) | Diagnostica un exponente que aparece sin punto y parte decimal obligatorios. | T07 G07 |
| [sa_integer_literal_error](../src/lexer_actions.c#L244) | Distingue sufijo entero mal escrito y sufijo ausente; informa el error y devuelve YYerror. | T03 G07 |
| [sa_float_exponent_error](../src/lexer_actions.c#L256) | Diagnostica exponente SINGLEF incompleto o mal formado. | T07 G07 |
| [display_character](../src/lexer_actions.c#L264) | Devuelve una representación legible del carácter, incluidos blancos y controles, para usarla en mensajes. | G07 |
| [sa_error](../src/lexer_actions.c#L278) | Informa el carácter inválido y devuelve YYerror para iniciar recuperación. | G01 G07 |
| [sa_operator_error](../src/lexer_actions.c#L284) | Diagnostica un operador incompleto que requería el segundo carácter =. | G01 G07 |

### main.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [main](../src/main.c#L19) | Procesa opciones y ruta, abre la entrada, inicia módulos, invoca el parser y la especialización, muestra resultados y libera recursos. Devuelve éxito sólo sin errores. | G03 G05 G07 G10 |

### numeric.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [numeric_error_message](../src/numeric.c#L4) | Recibe un código numérico interno y devuelve un literal prestado del mensaje correspondiente; no lo imprime. | G07 EC ED EG |
| [numeric_integer](../src/numeric.c#L10) | Comprueba finitud, rango int16 y ausencia de fracción; devuelve número y error explícito sin emitir código. | T03 T34 EG |
| [numeric_binary](../src/numeric.c#L16) | Calcula una operación/comparación con reglas int16/f32; detecta suma flotante/producto entero y devuelve resultado o código de error. | T03 T07 EC ED |

### rpn.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [operator_text](../src/rpn.c#L6) | Devuelve el nombre visible de un operador del enum o NULL si no pertenece al conjunto admitido. | G05 G07 |
| [rpn_init](../src/rpn.c#L15) | Inicializa una secuencia vacía sin reservar todavía pasos. | G05 |
| [rpn_free](../src/rpn.c#L19) | Libera el array de pasos y deja la secuencia vacía; los símbolos prestados continúan en la TS. | G02 G05 |
| [rpn_append_step](../src/rpn.c#L24) | Amplía el array cuando hace falta, añade un paso y devuelve su dirección desde uno. | G05 |
| [rpn_append_symbol](../src/rpn.c#L44) | Agrega la referencia prestada a un símbolo al cuerpo y devuelve su dirección desde uno. El llamador debe proporcionar un símbolo válido. | G05 |
| [rpn_append_destination](../src/rpn.c#L49) | Añade una referencia marcada como destino de asignación. | G05 |
| [rpn_append_operator](../src/rpn.c#L54) | Valida el operador y agrega su paso; devuelve cero si el operador no es válido. | G05 |
| [rpn_reserve_target](../src/rpn.c#L59) | Añade un destino de salto pendiente con valor cero para completarlo después. | G05 |
| [rpn_append_sequence](../src/rpn.c#L63) | Copia pasos de otra secuencia reubicando destinos y comienzos de control por el desplazamiento de inserción. | G05 |
| [rpn_patch_target](../src/rpn.c#L76) | Completa una reserva una sola vez y verifica direcciones y rango, admitiendo el primer paso posterior al cuerpo. | G05 |
| [rpn_is_complete](../src/rpn.c#L85) | Comprueba que todos los destinos estén resueltos y en rango; no comprueba por sí sola tipos ni equilibrio de operandos. | G05 |
| [print_symbol](../src/rpn.c#L96) | Imprime nombre o literal; entrecomilla y escapa CHAIN para que no se confunda con operadores o direcciones. | G05 G07 T10 |
| [rpn_print](../src/rpn.c#L122) | Imprime cada clase de paso con dirección local y referencias legibles; falla si hay destinos incompletos o error de escritura. | G05 G07 |

### scope.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |

| [qualified_name](../src/scope.c#L34) | Construye un nombre nuevo con la forma nombre:ámbito; quien lo recibe debe liberar ese texto. | G04 |
| [push_scope](../src/scope.c#L40) | Crea un ámbito con ruta, clase y padre, lo conserva en la lista de asignados y lo convierte en actual. | G04 |
| [scope_init](../src/scope.c#L58) | Activa identificadores calificados e inicia el ámbito global main. | G02 G04 |
| [scope_destroy](../src/scope.c#L64) | Libera todos los ámbitos y sus rutas al terminar su uso. | G04 G10 |
| [scope_lookup_local](../src/scope.c#L74) | Busca un nombre únicamente en el ámbito indicado construyendo su nombre calificado temporal. | G04 |
| [declare](../src/scope.c#L81) | Rechaza una redeclaración local o crea el símbolo con tipo, uso, propietario y línea; lo agrega a los metadatos del dueño. | G02 G04 |
| [scope_program](../src/scope.c#L114) | Declara el nombre del programa con uso PROGRAM. | G02 G04 |
| [scope_begin](../src/scope.c#L118) | Declara función, método o clase y abre su cuerpo; ante declaración rechazada usa un ámbito aislado de recuperación. | G03 G04 G06 |
| [scope_end](../src/scope.c#L133) | Restaura el ámbito padre y desactiva los ámbitos abiertos hasta la guarda indicada; también sirve al descarte por Bison. | G03 G04 |
| [scope_symbol](../src/scope.c#L146) | Devuelve el símbolo asociado a un ámbito o NULL si no hay uno válido. | G04 |
| [scope_in_function](../src/scope.c#L150) | Determina si RET está en una función o método activo, sin atravesar indebidamente un contexto de clase. | G03 G06 |
| [scope_resolve](../src/scope.c#L158) | Busca desde el ámbito actual hacia padres, incluyendo miembros heredados cuando corresponde; comprueba categoría y diagnostica inexistencia o mal uso. | G04 T32 |
| [scope_named_type](../src/scope.c#L189) | Resuelve un nombre como clase y devuelve la descripción de tipo que usarán las declaraciones. | G02 G04 |
| [scope_names_create](../src/scope.c#L194) | Crea la lista temporal de identificadores de una declaración y añade el primero con su línea. | G03 G04 |
| [scope_names_append](../src/scope.c#L199) | Añade identificador y línea al final de la lista temporal, preservando orden. | G03 G04 |
| [scope_names_free](../src/scope.c#L209) | Libera la lista temporal sin liberar los símbolos léxicos prestados. | G03 G10 |
| [scope_declare_names](../src/scope.c#L220) | Recibe lista propia de nombres, tipo y marca comptime; declara variables o atributos según contexto y consume la lista. | G02 G04 T22 |
| [scope_parameter](../src/scope.c#L230) | Recibe nombre, tipo, marca comptime y línea; declara formal, conserva orden y devuelve referencia prestada o NULL. | G02 G06 T22 |
| [scope_base](../src/scope.c#L236) | Resuelve la base visible, registra su referencia e invalida el esquema efectivo previo. | G04 T32 |
| [scope_object](../src/scope.c#L241) | Resuelve una variable o atributo y exige que tenga tipo de clase; devuelve el objeto o diagnostica el receptor incorrecto. | G04 T29 |

### specialize.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |

| [sp_find_template](../src/specialize.c#L9) | Busca la declaración fuente de una función, normalizando antes una referencia a instancia especializada. | G06 T22 |


| [specialize_init](../src/specialize.c#L16) | Inicializa la sesión privada con registros vacíos, presupuesto y profundidad; reinicia ubicaciones lógicas y orígenes diagnósticos. Conserva la API pública. | G05 T22 |
| [specialize_register](../src/specialize.c#L23) | Registra una función fuente con referencia prestada a su cuerpo retenido. | G05 G06 T22 |
| [specialize_program](../src/specialize.c#L31) | Coordina análisis de efectos, transformación del principal e instanciación de funciones ordinarias no usadas; registra valores comptime finales en TS. El estado pertenece a la sesión. | G05 G06 T22 |
| [specialize_print](../src/specialize.c#L51) | Imprime principal, instancias ejecutables y plantillas contextuales, incluidas las no invocadas; comprueba saltos completos mediante rpn_print. | G05 G07 T22 |
| [specialize_evaluate_call](../src/specialize.c#L69) | Consulta o genera una especialización con los argumentos recibidos; devuelve su valor abstracto y aplica efectos al entorno suministrado por el analizador de tipos. Una fuente activa devuelve desconocido sin expandir recursión. Puede registrar cuerpos y símbolos, pero no agrega una llamada al cuerpo ejecutable original. | G06 G08 T22 EI |
| [specialize_source_count](../src/specialize.c#L90) | Cuenta declaraciones registradas; no crea cuerpos ni modifica el registro. | G05 G08 |
| [specialize_source_body](../src/specialize.c#L96) | Devuelve cuerpo fuente prestado por índice y su símbolo mediante parámetro de salida; NULL si no existe. | G05 G08 |
| [specialize_instance_count](../src/specialize.c#L104) | Cuenta instancias concretas del registro; no especializa nuevas llamadas. | G05 G09 |
| [specialize_instance_body](../src/specialize.c#L110) | Devuelve cuerpo concreto prestado por índice y símbolo de instancia; NULL si no existe. | G05 G09 |
| [specialize_main_body](../src/specialize.c#L118) | Devuelve referencia prestada al principal transformado, válido hasta specialize_destroy. | G05 G09 |
| [specialize_has_comptime_effects](../src/specialize.c#L122) | Delega al análisis de efectos sobre la sesión actual; conserva la consulta pública usada por el chequeo de rangos descartados. | G08 T14 T22 |
| [specialize_requires_iteration](../src/specialize.c#L126) | Delega al explorador sobre una copia privada del entorno, sin emitir IR, instancias ni diagnósticos exploratorios. Conserva la consulta pública. | G08 T14 T22 |
| [specialize_destroy](../src/specialize.c#L131) | Libera identidades, principal, instancias y plantillas mediante sus propietarios, limpia orígenes y reinicia la sesión. No libera cuerpos fuente ni símbolos prestados. | G10 T22 |

### specialize_effects.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [sp_forget_call_writes](../src/specialize_effects.c#L5) | Invalida conocimiento de variables y objetos que una llamada puede modificar, usando su resumen de escrituras. | G06 T22 |
| [sp_apply_effects](../src/specialize_effects.c#L19) | Invalida escrituras y copia efectos desde un entorno posterior al llamador suministrado, remapeando el receptor cuando corresponde. La misma regla sirve al transformador y al explorador privado. | G06 T22 T24 |
| [sp_has_comptime_effects](../src/specialize_effects.c#L41) | Detecta escrituras comptime, copias de objetos afectados y llamadas con efectos externos de compilación. Se combina con el análisis independiente de requisitos por iteración. | G08 T14 T22 |
| [record_runtime_write](../src/specialize_effects.c#L61) | Añade una escritura no local ordinaria al resumen de efectos de la función, evitando duplicados. | G06 T22 |
| [analyze_function_effects](../src/specialize_effects.c#L69) | Recorre el cuerpo fuente para clasificar dependencias comptime, efectos externos, atributos y escrituras runtime. | G06 T22 |
| [propagate_function_effects](../src/specialize_effects.c#L104) | Propaga resúmenes por las llamadas hasta alcanzar punto fijo; no ejecuta ninguna función del usuario. | G06 T22 |
| [sp_analyze_effects](../src/specialize_effects.c#L132) | Calcula resúmenes locales y los propaga por llamadas hasta punto fijo; marca las declaraciones contextuales antes de transformar sus cuerpos. | G06 T22 |

### specialize_instances.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [sp_find_active_instance](../src/specialize_instances.c#L7) | Busca una instancia de la misma declaración fuente que todavía se está expandiendo; devuelve una referencia prestada o NULL. | G06 T22 EI |
| [matches_environment](../src/specialize_instances.c#L14) | Decide si una instancia contextual puede reutilizarse comparando receptor, entrada bajo control desconocido y asociaciones comptime. | G06 T22 |
| [prepare_call_environment](../src/specialize_instances.c#L35) | Copia el entorno del llamador, invalida valores ordinarios/locales y vincula parámetros/campos comptime de argumentos. Devuelve entorno propio. | G06 T19 T22 |
| [find_instance](../src/specialize_instances.c#L51) | Busca una instancia de la declaración compatible con entorno, receptor y contexto de entrada; devuelve referencia prestada o NULL. | G06 T22 |
| [create_instance](../src/specialize_instances.c#L61) | Reserva instancia activa y cuerpo, copia el entorno inicial y crea símbolo especializado si depende de contexto; enlaza la instancia al registro propietario. | G02 G05 T22 |
| [record_instance_bindings](../src/specialize_instances.c#L95) | Registra valores comptime finales locales en metadatos de instancia; usa referencias a constantes poseídas por TS. | G02 T22 |
| [sp_instantiate](../src/specialize_instances.c#L110) | Prepara entorno de activación y formales comptime; reutiliza o crea instancia, transforma su cuerpo y registra efectos, retorno y valores finales. | G06 T22 |
| [sp_destroy_instances](../src/specialize_instances.c#L136) | Libera cuerpos, entornos iniciales/finales y nodos de todas las instancias de la sesión; reinicia su lista y cola, sin liberar símbolos de TS ni fuentes prestadas. | G10 T22 |

### specialize_probe.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [probe_push](../src/specialize_probe.c#L56) | Añade un operando al análisis exploratorio; copia el entorno de campos de objetos para preservar el orden de evaluación. | T18 T19 T22 |
| [probe_pop](../src/specialize_probe.c#L63) | Retira un operando exploratorio; devuelve un valor desconocido si no hay operandos. | T22 |
| [probe_destroy](../src/specialize_probe.c#L67) | Libera pila, capturas de entornos, llamadas pendientes y entorno privado de la exploración. | G10 T22 |
| [probe_copy_object](../src/specialize_probe.c#L80) | Copia campos por posición en el estado exploratorio, incluidos objetos anidados; marca requisitos comptime desconocidos sin diagnósticos ni capturas en TS. Limita profundidad y trabajo. | T19 T22 T24 |
| [probe_invoke](../src/specialize_probe.c#L119) | Comprueba argumentos comptime y analiza el cuerpo fuente en un entorno privado, con formales ordinarios desconocidos. Devuelve conocimiento y efectos abstractos sin crear instancias ni emitir código. | G06 T19 T22 |
| [probe_assign](../src/specialize_probe.c#L148) | Actualiza argumentos pendientes o el entorno privado según el destino; copia campos de objetos desde la captura del operando. El llamador libera los operandos. | T18 T19 T22 T24 |
| [probe_operator](../src/specialize_probe.c#L175) | Consume operandos para aritmética, asignación, conversión, DUP, retorno y descarte de efectos runtime; libera sus capturas y conserva el conocimiento abstracto. | G05 T18 T22 |
| [probe_step](../src/specialize_probe.c#L204) | Despacha símbolos, destinos, atributos y llamadas del análisis privado; delega operadores y no emite instrucciones ni crea instancias ejecutables. | G05 T19 T22 |
| [probe_condition](../src/specialize_probe.c#L276) | Analiza la expresión de una condición sobre el estado privado y retira su valor de la pila. | T14 T22 |
| [probe_loop](../src/specialize_probe.c#L283) | Prueba un cuerpo con conocimiento ordinario olvidado en una copia aislada; recorre iteraciones conocidas si son necesarias y respeta el presupuesto compartido. | T14 T22 |
| [probe_if](../src/specialize_probe.c#L319) | Selecciona un brazo conocido o analiza ambos sobre entornos privados y combina su conocimiento y detención. Conserva los retornos abstractos. | G08 T22 |
| [probe_control](../src/specialize_probe.c#L342) | Evalúa la condición y delega a probe_loop o probe_if; no mezcla la decisión de expansión con la unión de ramas. | T14 T22 |
| [probe_range](../src/specialize_probe.c#L348) | Recorre un rango estructurado del cuerpo fuente con un presupuesto de trabajo compartido; se detiene ante retorno o requisito desconocido. | G05 T14 T22 |
| [sp_requires_iteration](../src/specialize_probe.c#L362) | Olvida valores ordinarios en una copia del entorno y comprueba si el cuerpo necesita conocimiento por iteración para satisfacer requisitos comptime; no emite IR, instancias ni diagnósticos. | G08 T14 T22 |

### specialize_transform.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [push](../src/specialize_transform.c#L55) | Añade un valor a la pila temporal de evaluación, aumentando el array. | T22 |
| [pop](../src/specialize_transform.c#L60) | Retira el último valor; una pila vacía genera diagnóstico de expresión intermedia inválida. | G05 T22 |
| [emit](../src/specialize_transform.c#L66) | Añade un paso al cuerpo de salida de la transformación. | G05 T22 |
| [discard](../src/specialize_transform.c#L70) | Elimina un fragmento descartable al final de la salida o emite DROP cuando deben preservarse sus efectos. | T18 T22 |
| [materialize_object](../src/specialize_transform.c#L75) | Emite MATERIALIZE con la captura obtenida y asocia esa captura al valor temporal. | G05 T22 T24 |
| [record_return](../src/specialize_transform.c#L83) | Acumula el valor de retorno; conserva uno conocido sólo si los retornos observados coinciden. | G06 T22 |
| [process_condition](../src/specialize_transform.c#L89) | Transforma el rango de condición y retira su valor de la pila temporal; conserva sus efectos emitidos. | G05 T22 |
| [check_discarded](../src/specialize_transform.c#L94) | Audita un rango que se eliminará con una copia del entorno y el receptor actual; conserva aisladas asignaciones y efectos de llamadas en todas las opciones de salida. | G08 T22 |
| [process_if](../src/specialize_transform.c#L100) | Evalúa la condición; audita el brazo descartado cuando es conocida, o conserva ambos brazos y une entornos. Mantiene comprobaciones dependientes antes de eliminar código. | G05 G08 T22 |
| [process_loop](../src/specialize_transform.c#L142) | Evalúa la condición, audita cuerpos descartados y expande ciclos por escrituras comptime o argumentos que necesitan valores de cada iteración. Mantiene el control externo desconocido, conserva ciclos invariantes runtime y detiene expansiones con errores o presupuesto agotado. | G05 G08 T14 T22 |
| [transform_control](../src/specialize_transform.c#L197) | Procesa IF o ciclo: selecciona ramas conocidas, expande ciclos por efectos o requisitos comptime, o conserva bifurcaciones; combina entornos. | G05 T14 T22 |
| [evaluate_binary](../src/specialize_transform.c#L204) | Calcula conocimiento del resultado si ambos operandos conocidos tienen tipo compatible; emite igualmente la operación runtime y conserva sus límites. | T03 T07 T22 |
| [assign_comptime](../src/specialize_transform.c#L215) | Valida control conocido, valor, tipo y error numérico; registra la asociación comptime en entorno o argumentos pendientes, sin escribir una variable runtime. | T22 EC ED EG |
| [assign_value](../src/specialize_transform.c#L234) | Procesa destinos ordinarios, formales y atributos: vincula comptime o emite la escritura runtime y actualiza/invalida el conocimiento correspondiente. | G06 T18 T22 T24 |
| [process_symbol](../src/specialize_transform.c#L282) | Consulta constante/entorno, sustituye lecturas comptime y conserva identidad de objetos; emite operando y captura cuando corresponde. | G05 T22 T24 |
| [resolve_attribute](../src/specialize_transform.c#L312) | Resuelve receptor concreto, índice y campo; actualiza el índice o captura de la instrucción y registra diagnósticos necesarios. Conserva su receptor original para el backend. | G07 T22 T24 T29 |
| [process_attribute](../src/specialize_transform.c#L345) | Usa el acceso ya resuelto para preparar un destino o emitir una lectura; conserva objetos/capturas y sustituye campos comptime conocidos. | G05 T22 T24 T29 |
| [begin_call](../src/specialize_transform.c#L382) | Crea un registro propio de llamada pendiente con receptor concreto, argumentos y origen FRAME; resuelve receptores anidados para el entorno y conserva el símbolo original emitido. | G06 T19 |
| [process_call](../src/specialize_transform.c#L393) | Elige instancia y emite CALL. Una reentrada a una fuente activa usa esa instancia con retorno desconocido; conserva la guardia runtime y evita especialización infinita. Libera la llamada pendiente. | G06 T19 T22 EI |
| [process_operator](../src/specialize_transform.c#L425) | Despacha aritmética, conversiones, asignaciones, DUP/DROP, salida y retorno; actualiza pila, conocimiento y detención, conservando efectos runtime. | G05 T18 T22 |
| [transform_range](../src/specialize_transform.c#L463) | Recorre pasos y controles con origen diagnóstico; delega operaciones y audita el resto del rango antes de detenerse por RET. | G05 G07 G08 T22 |
| [sp_transform](../src/specialize_transform.c#L518) | Recibe la sesión explícita y crea un contexto privado de recorrido, con pila, controles y llamadas pendientes. Transforma el cuerpo, devuelve conocimiento de retorno y libera temporales. | G05 T22 |


### symbol_table.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [symbol_list_append](../src/symbol_table.c#L8) | Añade una referencia no nula a una lista dinámica de metadatos; no duplica ni adquiere el símbolo referenciado. | G02 |
| [free_symbol](../src/symbol_table.c#L19) | Libera texto y arrays poseídos por un símbolo, después el nodo; no libera los símbolos que referencian esos arrays. | G02 G10 |
| [print_list](../src/symbol_table.c#L36) | Imprime una lista rotulada de nombres para parámetros, atributos, métodos o bases. | G02 G07 |
| [hash_func](../src/symbol_table.c#L46) | Calcula el hash djb2 del nombre para elegir un bucket; es una decisión de almacenamiento, no una regla del lenguaje. | G02 |
| [map_create](../src/symbol_table.c#L55) | Reserva tabla y buckets dinámicos e inicializa el modo de identificadores; devuelve la tabla propietaria de futuros nodos. | G02 |
| [map_get](../src/symbol_table.c#L72) | Busca coincidencia de nombre y categoría en el bucket y devuelve un símbolo prestado o NULL. | G02 |
| [map_put](../src/symbol_table.c#L83) | Reutiliza una entrada equivalente o crea un nodo estable con copias del nombre y categoría. | G02 |
| [add_to_symbol_table](../src/symbol_table.c#L112) | Recibe lexema/categoría y devuelve la referencia estable prestada que interna map_put en TS. | G02 |
| [map_free](../src/symbol_table.c#L116) | Libera todos los símbolos, buckets y tabla, una vez terminado su uso por otros módulos. | G02 G10 |
| [map_print](../src/symbol_table.c#L129) | Muestra símbolos y metadatos semánticos, omitiendo registros léxicos de identificadores cuando está activo el modo de ámbitos. | G02 G07 |
| [print_symbol_table](../src/symbol_table.c#L190) | Si existe TS, delega la presentación de lexemas y metadatos en map_print; no altera símbolos. | G02 G07 |
| [init_symbol_table](../src/symbol_table.c#L196) | Crea la tabla global de 256 buckets si todavía no existe; las cadenas de nodos mantienen dinámica la cantidad de símbolos. | G02 |
| [map_purge_provisional](../src/symbol_table.c#L203) | Elimina nodos numéricos provisionales después de yyparse, cuando el parser ya no necesita esas referencias. | G02 T03 |

### transition_table.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [get_col](../src/transition_table.c#L23) | Clasifica el carácter en una columna de las matrices, distinguiendo i, s, dígitos, puntuación, blancos y otros. | G01 T03 T07 T10 T15 |

### types.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [type_of_symbol](../src/types.c#L10) | Consulta tipo declarado/categoría y referencia de clase; devuelve descripción de tipo sin deducirlo del nombre calificado. | G02 G08 |
| [type_name](../src/types.c#L21) | Devuelve texto prestado para mostrar un tipo o el nombre de su clase; no imprime ni reserva memoria. | G07 G08 |
| [type_of_attribute](../src/types.c#L27) | Resuelve el tipo de un índice constante, el conjunto de candidatos de uno ordinario o una obligación comptime pendiente; no ejecuta el acceso. | G08 T29 |
| [types_compatible](../src/types.c#L49) | Comprueba todos los candidatos de origen/destino y compatibilidad de clases; permite posponer tipos comptime pendientes y suprime cascadas de TYPE_ERROR. | G08 T24 T29 |
| [numeric](../src/types.c#L68) | Devuelve si un tipo o todos sus candidatos admiten operación numérica; pendiente/error se aceptan provisionalmente para evitar cascadas. | G08 |
| [binary_type](../src/types.c#L78) | Recibe operador y tipos; informa incompatibilidad o devuelve tipo de resultado, conservando dependencia comptime en aritmética. | G08 |
| [check_body](../src/types.c#L88) | Recorre un cuerpo o slice con pila de tipos y origen fuente; comprueba asignaciones, ARG, RET, operadores y controles. Restaura el origen diagnóstico anterior y libera su pila. | G05 G07 G08 |
| [types_check_body](../src/types.c#L164) | Inicia la comprobación de un cuerpo completo usando sus pasos como origen diagnóstico; devuelve si aparecieron errores nuevos. | G08 |
| [types_check_sources](../src/types.c#L168) | Comprueba contención finita y tipos de principal y declaraciones fuente. Posponen índices comptime pendientes sin inventar argumentos para plantillas sin invocar. | G08 T22 T24 |
| [clear_type_snapshots](../src/types.c#L222) | Libera las capturas de objetos consumidas y reinicia su contador; se llama al terminar una iteración con pila vacía o al destruir el contexto. | G08 G10 T24 |
| [push_type_value](../src/types.c#L227) | Agrega un operando de análisis; para lecturas de objetos conserva una copia del entorno en ese instante. El contexto posee las capturas hasta consumirlas y liberar el grupo. | G08 T19 T22 T24 |
| [resolve_step](../src/types.c#L236) | Propaga valores, llamadas, asignaciones y copias de objetos en el entorno aislado de tipos. Resuelve índices comptime y consulta el especializador para retornos y efectos; no emite instrucciones en el cuerpo ejecutable original. | G06 G08 T19 T22 T24 T29 |
| [check_resolved_range](../src/types.c#L335) | Comprueba los tipos de un rango cuyos índices ya fueron resueltos, conservando la función y el paso fuente para los diagnósticos. El rango debe contener instrucciones completas. | G07 G08 T22 |
| [same_type_environment](../src/types.c#L340) | Compara asociaciones de ambos entornos para detectar estabilidad del conocimiento numérico entre iteraciones; no modifica los valores. | G08 T14 T22 |
| [resolve_loop](../src/types.c#L351) | Resuelve y comprueba cada iteración conocida de un ciclo con efectos o requisitos comptime en el entorno aislado. Conserva el estado final, combina conocimiento si el control se vuelve desconocido y termina ante error, condición falsa, estabilidad o límite de 100000 iteraciones por auditoría. | G07 G08 T14 T22 |
| [resolve_control](../src/types.c#L379) | Revisa ambas alternativas con copias del entorno y selecciona o une el conocimiento posterior. Los ciclos conocidos con efectos o requisitos comptime se revisan por iteración; los demás conservan el análisis conservador de una vuelta. | G08 T14 T22 |
| [resolve_range](../src/types.c#L409) | Recorre controles y pasos del rango fuente conservando su origen de diagnóstico; resuelve índices antes de comprobar tipos. | G07 G08 T22 |
| [types_check_discarded](../src/types.c#L420) | Recibe rango, función, entorno y receptor; copia pasos y entorno, comprueba índices/tipos incluidos tras RET y libera pilas, llamadas y capturas temporales. Conserva aislados los efectos sobre valores. | G07 G08 T22 T29 |
| [types_check_specializations](../src/types.c#L180) | Comprueba tipos del principal especializado y de todas las instancias ejecutables; resuelve obligaciones dependientes de comptime antes de seleccionar la salida. | G08 T22 |

### wasm_emit.c

| Función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| [write_line](../src/wasm_emit.c#L30) | Escribe una línea WAT formateada; durante planificación no produce salida. No ejecuta instrucciones del programa. | G09 |

| [body_id](../src/wasm_emit.c#L39) | Consulta el ID del cuerpo actual en layout; devuelve cero para el principal. | G09 |
| [wasm_type](../src/wasm_emit.c#L40) | Devuelve literal prestado f32 para SINGLEF e i32 para otras representaciones admitidas por el traductor. | G08 G09 |
| [new_local](../src/wasm_emit.c#L41) | Reserva ID de auxiliar; en planificación registra tipo y símbolo AUXILIARY en TS. Devuelve ID, sin emitir su asignación. | G02 G09 |
| [save_local](../src/wasm_emit.c#L56) | Crea auxiliar y emite local.set para guardar el valor calculado; devuelve ID del auxiliar, no el valor del programa. | G09 |
| [get_local](../src/wasm_emit.c#L61) | Emite local.get del auxiliar indicado; devuelve void y no lee una variable del compilador. | G09 |
| [pop](../src/wasm_emit.c#L62) | Retira referencia a operando del traductor; informa pila inválida si está vacía. No retira valores del ejecutable. | G05 G09 |
| [push](../src/wasm_emit.c#L66) | Añade descriptor de operando al array temporal de traducción; no emite instrucciones. | G05 G09 |
| [fail_if](../src/wasm_emit.c#L67) | Emite condición de fallo ya calculada, llamada a runtimeFail con código/línea y unreachable. No decide la condición de tipos. | G07 G09 EC ED EG EI |
| [integer_type](../src/wasm_emit.c#L70) | Construye y devuelve descripción estática INTEGER sin reserva de memoria. | G08 G09 |
| [frame_address](../src/wasm_emit.c#L71) | Emite recorrido de enlaces léxicos desde el marco actual hasta el dueño pedido; informa una relación léxica imposible. | G04 G06 G09 |
| [receiver_address](../src/wasm_emit.c#L81) | Encuentra la función método exterior y emite carga del receptor de su marco; informa receptor ausente. | G06 G09 T32 |
| [symbol_address](../src/wasm_emit.c#L88) | Emite dirección de símbolo por marco o identidad de atributo sobre receptor; consulta layout y no reconstruye ámbitos desde texto. | G04 G09 T29 T32 |
| [object_type](../src/wasm_emit.c#L97) | Consulta si un tipo o todos sus candidatos son objetos; no modifica metadatos. | G08 G09 T24 |
| [snapshot](../src/wasm_emit.c#L104) | Emite copia temporal de objeto en la reserva preplanificada del paso; devuelve auxiliar de la nueva dirección. | G06 G09 T19 T24 |
| [read_symbol](../src/wasm_emit.c#L111) | Emite literal o lectura de símbolo, guarda su valor en auxiliar y copia objetos para conservar el instante de evaluación; devuelve descriptor de operando. | G09 T19 T24 |
| [attribute](../src/wasm_emit.c#L132) | Emite índice/dirección y lectura o destino; aplica captura, conserva valor/categoría y copia objetos cuando corresponde. Añade descriptor a la pila del traductor. | G09 T22 T29 |
| [find_body](../src/wasm_emit.c#L161) | Busca cuerpo concreto por identidad; devuelve referencia prestada o informa ausencia de destino ejecutable. | G05 G09 |
| [initialize_frame](../src/wasm_emit.c#L166) | Emite inicialización de headers para objetos locales y formales del marco pendiente; no reserva marcos durante la compilación C. | G06 G09 T24 |
| [begin_call](../src/wasm_emit.c#L176) | Emite reserva/inicialización del marco pendiente, enlace léxico y receptor; apila símbolo y auxiliar del marco. | G06 G09 T19 |
| [call_function](../src/wasm_emit.c#L191) | Emite llamada al cuerpo concreto, guarda retorno en auxiliar y restaura sp del llamador; retira el marco pendiente del traductor. | G06 G09 T19 |
| [materialize](../src/wasm_emit.c#L200) | Emite escrituras de constantes capturadas y recorre capturas hijas sobre la copia de objeto; conserva el original. | G09 T19 T22 T24 |
| [binary](../src/wasm_emit.c#L223) | Retira dos operandos, emite operación y auxiliar tipado; incluye controles c/d y normalización int16 antes de devolver descriptor a la pila. | G08 G09 EC ED |
| [convert_integer](../src/wasm_emit.c#L253) | Emite comprobaciones de finitud, rango y fracción antes de la conversión; produce auxiliar INTEGER o fallo del módulo. | G09 T34 EG |
| [output](../src/wasm_emit.c#L264) | Emite import de impresión por tipo; para selección heterogénea usa categoría y valor ya capturados sin releer el objeto. | G09 T10 T29 |
| [emit_operator](../src/wasm_emit.c#L275) | Despacha operadores de IR, asignación, retorno, duplicación y descarte; RET guarda resultado y salta al epílogo compartido. | G05 G09 T18 EI |
| [emit_control](../src/wasm_emit.c#L292) | Traduce región IF/FROM a control estructurado con etiquetas locales y destinos validados; devuelve primer paso posterior a la región. | G05 G09 T14 |
| [emit_step](../src/wasm_emit.c#L311) | Traduce un paso ordinario y sus metadatos, incluido CHECK_INDEX antes del RHS; modifica estado de traducción y escribe WAT. | G05 G09 T29 |
| [emit_range](../src/wasm_emit.c#L341) | Recorre pasos, localiza regiones por control_start y delega control o instrucción. No introduce un despachador runtime de PC. | G05 G09 |

| [prepare_bodies](../src/wasm_emit.c#L421) | Retiene referencias a principal/instancias y calcula tamaños de marcos/reservas temporales antes de emitir; informa límites de memoria. | G05 G09 |
| [write_body](../src/wasm_emit.c#L466) | Planifica auxiliares, declara locales y escribe entrada, cuerpo y epílogo; emite indicador de recursión por fuente y libera pilas C del cuerpo. | G02 G09 EI |
| [write_module](../src/wasm_emit.c#L497) | Escribe imports, memoria, datos, indicadores y todos los cuerpos concretos; las plantillas simbólicas no se emiten como funciones ejecutables. | G05 G09 |
| [wasm_write](../src/wasm_emit.c#L523) | Recibe una ruta tras el chequeo común de tipos; planifica layout y auxiliares y publica WAT por archivo temporal y rename. Libera recursos en éxito y fallo. | G07 G09 G10 |

## Funciones del soporte JavaScript

Estos nombres pertenecen al soporte de ejecución, no se cuentan entre las 300
funciones C anteriores. Los scripts assemble.mjs y run.mjs también tienen
coordinación a nivel de módulo, sin una función main propia.

| Archivo y función | Contrato y efectos | Requisitos |
| --- | --- | --- |
| runtime/host.mjs: execute | Recibe bytes y callback de salida, instancia el módulo, llama main una vez y devuelve la instancia mediante Promise. Propaga errores del módulo. | G09 G10 |
| runtime/host.mjs: printInteger | Import que entrega el INTEGER al callback del host; no calcula operaciones del lenguaje. | G09 T03 |
| runtime/host.mjs: printFloat | Import que entrega SINGLEF al callback; no reemplaza aritmética Wasm por JavaScript. | G09 T07 |
| runtime/host.mjs: printString | Lee bytes hasta el terminador en memoria exportada, decodifica UTF-8 y entrega el texto al callback. | G09 T10 |
| runtime/host.mjs: runtimeFail | Recibe código/línea, consulta mensaje y lanza excepción; el módulo emite además unreachable por si otro host retornara. | G07 EC ED EG EI |
| runtime/compile.mjs: run | Ejecuta una herramienta sin shell, hereda sus streams y propaga fallos/estado. No interpreta IR. | G09 G10 |
| runtime/run.mjs: callback de salida | Imprime cada valor o acumula resultados para JSON de pruebas; conserva el prefijo emitido antes de un error. | G09 G10 |
| runtime/index.html: manejador change | Lee el archivo seleccionado, llama execute y añade salida/error a la página. | G09 G10 |

## Herramientas de prueba

verify_tp3.py y verify_tp3_course.py comprueban recuperación, estructura de IR y
operaciones soportadas por su evaluador. Los controles runtime se verifican en
Wasm real mediante verify_tp4.py. compiler_cases.py comparte expectativas entre
las salidas y verify_outputs.py verifica diagnósticos y polaca comunes.
Exige el diagnóstico esperado para cada rechazo y rechaza los fallos de
sanitizadores aunque la salida sea 1; test_verify_tp4.py comprueba estos casos.
verify_tp4_browser.py prueba el selector del navegador si hay Chromium.
verify_function_docs.py enumera definiciones C, verifica cobertura única y líneas
vigentes de este inventario. Esta última prueba no demuestra la exactitud del
texto: las descripciones se revisan contra los cuerpos de las funciones.

## Operaciones compartidas y backend reorganizado

| Función | Responsabilidad | Etapa |
| --- | --- | --- |
| [comptime_select_field](../src/comptime_objects.c#L184) | Devuelve el atributo seleccionado por un índice entero conocido dentro de rango; de otro modo NULL. | Evaluación |
| [comptime_call_environment](../src/comptime_values.c#L141) | Copia el entorno del llamador, olvida valores ordinarios y reinicia locales antes de incorporar argumentos. | Evaluación |
| [comptime_validate_assignment](../src/comptime_values.c#L158) | Comprueba control, conocimiento y tipo de un valor comptime sin modificar el entorno ni crear instancias. | Semántica |
| [memory_resize](../src/memory.c#L5) | Reserva o amplía memoria; tamaño cero reserva un byte y un fallo termina la compilación. | Memoria |
| [memory_zero](../src/memory.c#L11) | Reserva memoria inicializada a cero con manejo común de errores. | Memoria |
| [is_operator](../src/rpn.c#L170) | Comprueba categoría y operador de un paso sin interpretar otros miembros de la unión. | Polaca |
| [rpn_describe_control](../src/rpn.c#L174) | Interpreta destinos de Polaca para describir condición, cuerpo, else y final de IF/FROM. | Polaca |
| [rpn_index_controls](../src/rpn.c#L196) | Construye un índice por posición de inicio para consultar controles sin búsquedas repetidas. | Polaca |
| [sp_evaluate_call](../src/specialize_probe.c#L377) | Valida tipos y evalúa una llamada sobre un entorno privado, propagando valores y efectos sin registrar instancias ejecutables. | Evaluación |
| [plan_body](../src/wasm_emit.c#L451) | Reserva pilas y planifica los locales Wasm antes de emitir el cuerpo. | Backend |
| [write_copy_helper](../src/wasm_emit.c#L399) | Emite copias estructurales entre clases compatibles conservando los esquemas de destino. | Backend |
| [write_memory_helpers](../src/wasm_emit.c#L349) | Emite el helper Wasm de reserva y limpieza de marcos. | Backend |
| [write_object_helpers](../src/wasm_emit.c#L357) | Emite inicialización de objetos, tamaños y búsqueda de atributos según el esquema real. | Backend |
| [probe_validate_object](../src/specialize_probe.c#L110) | Comprueba campos comptime capturados por una lectura sobre un entorno privado, sin emitir snapshots ni instancias. | Evaluación |
