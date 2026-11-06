# Informe de funciones y trazabilidad de requisitos

Fecha: 4 de noviembre de 2026. Se describe el código actual y, en una sección
separada, los contratos propuestos para completarlo. No se presenta código futuro
como implementado. El [plan de finalización](PLAN_FINALIZACION_COMPILADOR.md)
define el orden de cambios y los criterios de aceptación.

## Cómo leer este informe

Cada fila identifica una función propia, enlaza su definición, explica su efecto
y relaciona requisitos. Una función auxiliar aporta al requisito del módulo;
eso no significa que la cátedra exija ese nombre, algoritmo o estructura concreta.

Se cubren **190 definiciones**: las públicas y `static` de `src/*.c` y los dos
ayudantes C escritos en `grammar.y`. Los prototipos de headers no cuentan otra vez.
Las funciones automáticas de Bison y las funciones de biblioteca no son código
propio que haya que documentar una por una. Los verificadores Python se explican
al final como herramientas de prueba, fuera del inventario del compilador.

Requisitos resumidos; su formulación y páginas están en la matriz del plan:

| Código | Requisito |
| --- | --- |
| G01 | Léxico por matrices, tokens y reglas comunes. |
| G02 | TS dinámica, referencias, tipos, usos y metadatos. |
| G03 | Sintaxis, parser generado y recuperación sin conflictos. |
| G04 | Declaración, categoría, alcance y name mangling. |
| G05 | Polaca dinámica, completa, numerada y cuerpos separados. |
| G06 | Funciones/métodos, cantidad, copia de parámetros y retornos. |
| G07 | Diagnósticos con línea, IR/TS legibles y trazas optativas. |
| G08 | Chequeo completo de tipos en traducción Polaca → destino. |
| G09 | Código destino mediante auxiliares, archivo y ejecución. |
| G10 | Recursos, reproducibilidad, entrega y explicación del desarrollo. |
| T03 / T07 | INTEGER de 16 bits / SINGLEF de 32 bits. |
| T10 / T15 | Cadenas multilínea / comentarios de línea. |
| T14 / T18 | FROM / asignaciones dentro de expresiones. |
| T19 / T22 | Orden de argumentos / valores en compilación. |
| T24 / T29 / T32 | Clases con código / posiciones / herencia ambigua. |
| T34 | Conversión explícita TOI. |
| EC / ED / EG / EI | Suma SINGLEF / producto INTEGER / pérdida en conversión / recursión. |
| L01 | Legibilidad y explicación solicitadas por el grupo. |

El código actual cubre principalmente TP1–TP3. Las referencias a G09 en funciones
que construyen POUT o llamadas identifican su participación en la cadena de
traducción; no afirman que ya exista el backend de TP4. EC/ED/EG/EI aún no cuentan
con implementación de ejecución en este repositorio.

## Datos y recorrido general

`map_node_t` representa símbolos y es propiedad de la TS. Sus arrays de parámetros,
atributos y bases referencian otros símbolos sin poseerlos. `scope_t` conserva
padres y rutas; debe vivir mientras se consulta la información de declaraciones.
`rpn_t` posee pasos, pero éstos toman prestados símbolos de la TS.

`value_t`, `binding_t` y `environment_t`, internos de `specialize.c`, expresan
conocimiento durante compilación; no son las variables ni los marcos del programa
ejecutable. Los registros de plantilla prestan el cuerpo fuente; las instancias
poseen cuerpos transformados y sus entornos. `OBJECT_VALUE` conserva capturas
inmutables de campos comptime dentro de la TS.

Recorrido actual: `main` → `yyparse` → `yylex` y acciones de gramática →
`scope_*` y `codegen_*` → `codegen_finalize` → `specialize_program` → impresión
de Polaca y TS → liberación. `--analyze` suprime la impresión de Polaca, pero
no evita su construcción ni la evaluación semántica. Es distinto del chequeo
de tipos completo de TP4 que todavía se propone agregar.

## Funciones existentes

### main.c: Entrada y coordinación (2 funciones)

Posee el archivo abierto y coordina el ciclo de vida. Se propone extraer la secuencia de etapas a compiler.c; yyparse es el parser generado, no una función escrita aquí.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [main](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/main.c:15) | Procesa opciones y ruta, abre la entrada, inicia módulos, invoca el parser y la especialización, muestra resultados y libera recursos. Devuelve éxito sólo sin errores. | G03 G05 G07 G10 |
| [yyerror](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/main.c:84) | Atiende el callback de error de Bison: cierra el diagnóstico pendiente, incrementa el contador y muestra mensaje con línea. | G03 G07 |

### lexer.c: Lectura y reconocimiento (10 funciones)

Lee source_file y modifica estado de entrada, líneas, yylval y yylloc. Su interfaz pública con Bison continúa siendo yylex; los ayudantes son internos.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [read_input_char](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:19) | Lee un carácter y produce una sola nueva línea artificial al EOF para completar tokens con anticipación; informa si el carácter es sintético. | G01 |
| [update_line_number](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:34) | Actualiza la línea real y devuelve si avanzó; cuenta CRLF una sola vez e ignora el delimitador artificial. | G01 G07 |
| [is_recovery_delimiter](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:53) | Indica si un carácter permite detener la recuperación léxica sin consumir indefinidamente otras construcciones. | G01 G03 |
| [is_valid_token_boundary](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:58) | Reconoce puntuación que conviene preservar como posible comienzo o cierre de un token después de un error. | G01 G03 |
| [push_back_char](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:62) | Devuelve un carácter al archivo y corrige el avance de línea producido al leerlo; no devuelve EOF. | G01 |
| [recover_from_lexical_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:73) | Consume hasta un delimitador de recuperación, preserva ese delimitador cuando corresponde y vacía el lexema inválido. | G01 G03 G07 |
| [set_token_location](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:86) | Carga la ubicación del token en yylloc para que Bison y las acciones conserven sus líneas de origen. | G03 G07 |
| [log_token](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:93) | Muestra token, lexema y línea mediante la traza opcional; no genera otro token. | G07 |
| [lexer_reset_input](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:98) | Reinicia posición, tratamiento de EOF y búfer para comenzar el archivo desde la primera línea. | G01 |
| [yylex](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/lexer.c:106) | Ejecuta ambas matrices, aplica anticipación y recuperación, y devuelve un token, YYerror o EOF; las acciones cargan yylval. | G01 G02 G03 |

### transition_table.c: Matriz de transiciones (1 funciones)

Además de esta función define transition_table. Las columnas deben coincidir con sem_act_mat y los enums/macros del header; editar una requiere revisar las tres piezas.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [get_col](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/transition_table.c:23) | Clasifica el carácter en una columna de las matrices, distinguiendo i, s, dígitos, puntuación, blancos y otros. | G01 T03 T07 T10 T15 |

### semantic_actions.c: Acciones semánticas léxicas (25 funciones)

Las acciones reciben el carácter de la transición. Devuelven −1 para continuar internamente, un token o YYerror. −1 no debe salir de yylex como si fuera un error del parser. Los símbolos devueltos son prestados.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [ensure_lexeme_capacity](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:15) | Reserva o amplía el búfer dinámico para el tamaño solicitado; una falla de memoria termina con diagnóstico interno. | G01 G02 |
| [append_to_lexeme](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:37) | Añade un carácter y el terminador nulo, aumentando capacidad cuando hace falta. | G01 |
| [reset_lexeme_buffer](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:43) | Deja el lexema vacío, conservando la reserva reutilizable. | G01 |
| [free_lexeme_buffer](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:49) | Libera el búfer y reinicia puntero, capacidad y longitud. | G10 |
| [check_reserved_words](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:81) | Devuelve el token reservado para una palabra ya normalizada a minúsculas, o −1 si no es reservada. | G01 G03 |
| [sa_init](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:104) | Comienza un lexema con el carácter actual y devuelve la indicación interna de continuar. | G01 |
| [sa_append](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:110) | Agrega el carácter excepto CR/LF; permite que el texto de cadenas multilínea quede sin saltos. | G01 T10 |
| [add_to_symbol_table](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:117) | Interna lexema y categoría mediante map_put y devuelve la referencia estable al símbolo. | G02 |
| [sa_token_consume](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:121) | Devuelve directamente el token del carácter actual, consumiéndolo. | G01 |
| [sa_token_buffered](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:127) | Devuelve el token del carácter ya guardado; la transición permite devolver el carácter de anticipación. | G01 |
| [sa_ignore](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:135) | No altera el lexema ni devuelve token; se usa para blancos y comentarios. | G01 T15 |
| [sa_identifier](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:139) | Distingue reservadas sin importar mayúsculas; rechaza mayúsculas en identificadores, trunca a 22 con warning e interna el identificador válido. | G01 G02 |
| [sa_int_const](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:170) | Completa y valida $i y la magnitud; registra INTEGER y marca 32768 provisional para resolver su signo en el parser. | G02 T03 |
| [sa_float_const](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:189) | Convierte una copia del texto reemplazando s por e, valida formato y rango SINGLEF y registra la constante sin alterar el lexema fuente. | G02 T07 |
| [sa_init_chain](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:215) | Vacía el búfer al abrir una cadena para no almacenar su comilla inicial como contenido. | T10 |
| [sa_chain](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:220) | Registra el contenido de una cadena cerrada como CHAIN y devuelve su token. | G02 T10 |
| [sa_multi_char_op](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:225) | Completa y reconoce :=, >=, <=, == o !=; una combinación no admitida produce error léxico. | G01 |
| [sa_dot_or_float_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:236) | Distingue un punto aislado de un intento de flotante sin parte decimal; devuelve punto o error. | G01 T07 |
| [sa_float_decimal_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:243) | Diagnostica un exponente que aparece sin punto y parte decimal obligatorios. | T07 G07 |
| [sa_integer_literal_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:250) | Distingue sufijo entero mal escrito y sufijo ausente; informa el error y devuelve YYerror. | T03 G07 |
| [sa_float_exponent_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:262) | Diagnostica exponente SINGLEF incompleto o mal formado. | T07 G07 |
| [display_character](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:270) | Devuelve una representación legible del carácter, incluidos blancos y controles, para usarla en mensajes. | G07 |
| [sa_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:284) | Informa el carácter inválido y devuelve YYerror para iniciar recuperación. | G01 G07 |
| [sa_operator_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:290) | Diagnostica un operador incompleto que requería el segundo carácter =. | G01 G07 |
| [print_symbol_table](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/semantic_actions.c:296) | Si hay tabla, delega su presentación en map_print. | G02 G07 |

### symbol_table.c: Tabla de símbolos (12 funciones)

La TS posee nodos y textos. map_get/map_put devuelven punteros prestados estables. Sus nodos no se liberan al descartar un token Bison. Los arrays de metadatos se liberan sin destruir símbolos referenciados.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [symbol_list_append](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:8) | Añade una referencia no nula a una lista dinámica de metadatos; no duplica ni adquiere el símbolo referenciado. | G02 |
| [free_symbol](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:19) | Libera texto y arrays poseídos por un símbolo, después el nodo; no libera los símbolos que referencian esos arrays. | G02 G10 |
| [print_list](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:36) | Imprime una lista rotulada de nombres para parámetros, atributos, métodos o bases. | G02 G07 |
| [hash_func](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:46) | Calcula el hash djb2 del nombre para elegir un bucket; es una decisión de almacenamiento, no una regla del lenguaje. | G02 |
| [map_create](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:55) | Reserva tabla y buckets dinámicos e inicializa el modo de identificadores; devuelve la tabla propietaria de futuros nodos. | G02 |
| [map_get](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:72) | Busca coincidencia de nombre y categoría en el bucket y devuelve un símbolo prestado o NULL. | G02 |
| [map_contains](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:83) | Devuelve si existe la entrada, reutilizando map_get. | G02 |
| [map_put](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:87) | Reutiliza una entrada equivalente o crea un nodo estable con copias del nombre y categoría. | G02 |
| [map_free](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:116) | Libera todos los símbolos, buckets y tabla, una vez terminado su uso por otros módulos. | G02 G10 |
| [map_print](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:129) | Muestra símbolos y metadatos semánticos, omitiendo registros léxicos de identificadores cuando está activo el modo de ámbitos. | G02 G07 |
| [init_symbol_table](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:190) | Crea la tabla global de 256 buckets si todavía no existe; las cadenas de nodos mantienen dinámica la cantidad de símbolos. | G02 |
| [map_purge_provisional](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/symbol_table.c:197) | Elimina nodos numéricos provisionales después de yyparse, cuando el parser ya no necesita esas referencias. | G02 T03 |

### diagnostics.c: Diagnósticos y recuperación (16 funciones)

El contador representa errores detectados, no cantidad de impresiones. Distinguir diagnóstico pendiente, error léxico ya reportado y error independiente evita duplicaciones y pérdidas de mensajes al EOF.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [set_semantic_origin](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:29) | Cambia el origen cuerpo/paso del diagnóstico y devuelve el anterior para restaurarlo después de un recorrido anidado. | G07 T22 |
| [clear_semantic_origins](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:35) | Libera el registro de diagnósticos semánticos emitidos y reinicia el origen activo. | G07 |
| [set_trace_enabled](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:45) | Activa o desactiva la salida de trazas sin modificar los controles del lenguaje. | G07 |
| [trace_printf](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:49) | Imprime con argumentos variables únicamente si las trazas están habilitadas. | G07 |
| [report_semantic_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:57) | Informa y cuenta el error; si hay origen, evita repetir el mismo mensaje para el mismo cuerpo, paso y línea. | G04 G07 T22 |
| [flush_syntax_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:96) | Imprime un diagnóstico sintáctico pendiente, ya contabilizado, y desactiva su impresión futura. | G03 G07 |
| [finish_error_recovery](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:104) | Emite lo pendiente y limpia el estado de recuperación léxica/sintáctica. | G03 G07 |
| [report_lexical_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:109) | Imprime y cuenta un error léxico con línea y descripción. | G01 G07 |
| [begin_lexical_recovery](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:119) | Cierra un pendiente anterior y marca el nuevo episodio como originado en el léxico para evitar duplicación automática. | G01 G03 |
| [begin_syntax_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:126) | Registra línea, token inesperado y contexto; cuenta el error al detectarlo aunque no se alcance una producción recuperadora. | G03 G07 |
| [report_syntax_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:136) | Cuenta e imprime un error independiente detectado directamente por una producción diagnóstica. | G03 G07 |
| [report_recovered_syntax_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:142) | Sustituye la explicación del error pendiente por un mensaje específico, sin volver a contarlo. | G03 G07 |
| [report_recovered_syntax_error_with_context](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:149) | Añade el contexto pendiente al mensaje de recuperación y delega su emisión. | G03 G07 |
| [syntax_error_unexpected](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:159) | Devuelve el token inesperado registrado para especializar un diagnóstico del parser. | G03 G07 |
| [finish_statement](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:163) | Cierra la recuperación cuando no hay un error exterior pendiente ni un episodio léxico por resolver. | G03 |
| [is_lexical_recovery](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/diagnostics.c:169) | Indica si la recuperación actual se originó en el léxico. | G01 G03 |

### scope.c: Ámbitos y clases (31 funciones)

scope_names es temporal y las funciones de declaración lo consumen. Los ámbitos y símbolos retenidos duran hasta el cierre del compilador. Se propone mover herencia, esquemas y compatibilidad a classes.c conservando los contratos.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [allocate](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:33) | Reserva memoria inicializada para estructuras de ámbitos y termina con diagnóstico interno si falla. | G04 |
| [qualified_name](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:42) | Construye un nombre nuevo con la forma nombre:ámbito; quien lo recibe debe liberar ese texto. | G04 |
| [push_scope](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:48) | Crea un ámbito con ruta, clase y padre, lo conserva en la lista de asignados y lo convierte en actual. | G04 |
| [scope_init](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:66) | Activa identificadores calificados e inicia el ámbito global main. | G02 G04 |
| [scope_destroy](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:72) | Libera todos los ámbitos y sus rutas al terminar su uso. | G04 G10 |
| [lookup_in](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:82) | Busca un nombre únicamente en el ámbito indicado construyendo su nombre calificado temporal. | G04 |
| [declare](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:89) | Rechaza una redeclaración local o crea el símbolo con tipo, uso, propietario y línea; lo agrega a los metadatos del dueño. | G02 G04 |
| [scope_program](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:122) | Declara el nombre del programa con uso PROGRAM. | G02 G04 |
| [scope_begin](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:126) | Declara función, método o clase y abre su cuerpo; ante declaración rechazada usa un ámbito aislado de recuperación. | G03 G04 G06 |
| [scope_end](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:141) | Restaura el ámbito padre y desactiva los ámbitos abiertos hasta la guarda indicada; también sirve al descarte por Bison. | G03 G04 |
| [scope_symbol](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:154) | Devuelve el símbolo asociado a un ámbito o NULL si no hay uno válido. | G04 |
| [scope_in_function](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:158) | Determina si RET está en una función o método activo, sin atravesar indebidamente un contexto de clase. | G03 G06 |
| [scope_resolve](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:166) | Busca desde el ámbito actual hacia padres, incluyendo miembros heredados cuando corresponde; comprueba categoría y diagnostica inexistencia o mal uso. | G04 T32 |
| [scope_named_type](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:197) | Resuelve un nombre como clase y devuelve la descripción de tipo que usarán las declaraciones. | G02 G04 |
| [scope_names_create](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:202) | Crea la lista temporal de identificadores de una declaración y añade el primero con su línea. | G03 G04 |
| [scope_names_append](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:207) | Añade identificador y línea al final de la lista temporal, preservando orden. | G03 G04 |
| [scope_names_free](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:217) | Libera la lista temporal sin liberar los símbolos léxicos prestados. | G03 G10 |
| [scope_declare_names](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:228) | Declara cada identificador como atributo o variable según el contexto y consume la lista temporal. | G02 G04 |
| [scope_parameter](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:236) | Declara un formal con su tipo y lo agrega en orden al propietario. | G02 G06 |
| [scope_declare_comptime_names](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:240) | Declara variables o atributos y marca los símbolos admitidos como comptime; consume la lista temporal. | G02 T22 |
| [scope_class_code](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:250) | Copia el código escrito en la cabecera al símbolo de clase. | G02 T24 |
| [scope_base](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:257) | Resuelve la base visible, registra su referencia e invalida el esquema efectivo previo. | G04 T32 |
| [scope_object](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:265) | Resuelve una variable o atributo y exige que tenga tipo de clase; devuelve el objeto o diagnostica el receptor incorrecto. | G04 T29 |
| [find_methods](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:275) | Recorre miembros propios y después bases, acumulando candidatos y detectando ciclos mediante el camino activo; pese al nombre también encuentra atributos. | T32 |
| [inherited_symbol](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:288) | Resuelve un miembro heredado implícito; caminos al mismo símbolo no son ambiguos, pero declaraciones distintas homónimas sí. | G04 T32 |
| [scope_method](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:309) | Busca un método en la clase y bases, deduplica identidad, valida categoría y distingue ausencia, ambigüedad y ciclo. | G04 G06 T32 |
| [append_distinct](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:335) | Añade un símbolo a una lista sólo si esa misma referencia todavía no aparece. | T32 |
| [scope_class_layout](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:341) | Construye atributos efectivos heredados primero y propios después, deduplicando por identidad; registra estados de recorrido para detectar ciclos. | T24 T29 T32 |
| [scope_classes_compatible](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:367) | Compara cantidad y tipos en posiciones efectivas, usando identidad de clase y tipo declarado; no compara nombres ni métodos. | T24 |
| [scope_class_complete](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:378) | Finaliza el esquema y compara con otras clases completas de igual código, informando estructuras incompatibles. | T24 |
| [scope_attribute_at](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/scope.c:390) | Obtiene el atributo de una posición válida y aplica la comprobación de ambigüedad de nombres heredados. | T29 T32 |

### rpn.c: Almacenamiento y presentación de Polaca (13 funciones)

Las direcciones visibles empiezan en uno; count + 1 es salida válida de un cuerpo. La IR presta referencias de TS. Completar saltos no equivale a comprobar tipos o a emitir instrucciones máquina.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [operator_text](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:6) | Devuelve el nombre visible de un operador del enum o NULL si no pertenece al conjunto admitido. | G05 G07 |
| [rpn_init](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:15) | Inicializa una secuencia vacía sin reservar todavía pasos. | G05 |
| [rpn_free](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:19) | Libera el array de pasos y deja la secuencia vacía; los símbolos prestados continúan en la TS. | G02 G05 |
| [rpn_append_step](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:24) | Amplía el array cuando hace falta, añade un paso y devuelve su dirección desde uno. | G05 |
| [rpn_append_symbol](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:44) | Añade un operando que referencia un símbolo no nulo. | G02 G05 |
| [rpn_append_destination](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:49) | Añade una referencia marcada como destino de asignación. | G05 |
| [rpn_append_operator](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:54) | Valida el operador y agrega su paso; devuelve cero si el operador no es válido. | G05 |
| [rpn_reserve_target](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:59) | Añade un destino de salto pendiente con valor cero para completarlo después. | G05 |
| [rpn_append_sequence](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:63) | Copia pasos de otra secuencia reubicando destinos y comienzos de control por el desplazamiento de inserción. | G05 |
| [rpn_patch_target](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:76) | Completa una reserva una sola vez y verifica direcciones y rango, admitiendo el primer paso posterior al cuerpo. | G05 |
| [rpn_is_complete](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:85) | Comprueba que todos los destinos estén resueltos y en rango; no comprueba por sí sola tipos ni equilibrio de operandos. | G05 |
| [print_symbol](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:96) | Imprime nombre o literal; entrecomilla y escapa CHAIN para que no se confunda con operadores o direcciones. | G05 G07 T10 |
| [rpn_print](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/rpn.c:122) | Imprime cada clase de paso con dirección local y referencias legibles; falla si hay destinos incompletos o error de escritura. | G05 G07 |

### codegen.c: Constructor de Polaca (37 funciones)

No es todavía un emisor Pentium/WebAssembly. Los fragmentos son propios: combinarlos o emitirlos suele consumirlos; los cuerpos quedan retenidos hasta codegen_destroy. Las guardas restauran el contexto también en recuperación.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [codegen_init](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:24) | Inicializa el programa y el especializador; actualmente ignora enabled y siempre construye IR, incluso si luego no se imprime. | G05 T22 |
| [codegen_destroy](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:35) | Destruye especializaciones y cuerpos fuente, conservando prestados los símbolos hasta que main libere la TS. | G05 G10 |
| [codegen_print](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:50) | Delega la impresión de Polaca especializada en specialize_print. | G05 G07 |
| [codegen_finalize](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:54) | Ejecuta la especialización del programa después del parseo. | G05 T22 |
| [new_fragment](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:58) | Reserva e inicializa un fragmento de Polaca propietario de su array de pasos. | G05 |
| [checked_realloc](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:84) | Amplía las estructuras de llamadas con comprobación de fallo de memoria. | G05 G06 |
| [codegen_arguments_append](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:93) | Conserva un fragmento de expresión como siguiente argumento, manteniendo su posición original. | G06 T19 |
| [codegen_arguments_free](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:104) | Libera todos los fragmentos de argumentos y su contenedor. | G06 G10 |
| [codegen_order_append](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:111) | Guarda la siguiente constante del orden, incluyendo una posición inválida nula para no falsear cantidades durante recuperación. | T19 |
| [codegen_order_free](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:123) | Libera el array y contenedor del orden, sin liberar sus símbolos. | T19 G10 |
| [codegen_call_begin](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:129) | Guarda función, receptor, argumentos y línea; verifica cantidad de reales contra formales. | G06 T19 |
| [codegen_call_free](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:140) | Libera el contexto de llamada y los fragmentos que todavía posee. | G06 G10 |
| [codegen_call_end](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:146) | Valida orden INTEGER completo y único de 1..N; emite FRAME, fragmentos y copias ARG en ese orden, y CALL conservando asociación formal. | G06 T19 |
| [codegen_attribute](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:207) | Prepara receptor, índice y línea; verifica el esquema y, para índice INTEGER constante, existencia y ambigüedad de la posición. | T29 T32 |
| [codegen_attribute_value](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:225) | Construye un paso de lectura posicional, conservando receptor e índice; referencia el campo concreto si puede identificarlo. | G05 T29 |
| [codegen_attribute_assignment](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:238) | Añade destino posicional y asignación al fragmento derecho; emite el resultado en el cuerpo actual y consume ambos fragmentos. | G05 T29 |
| [codegen_class_end](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:251) | Delega la finalización de esquema y comprobación de clases con código en scope_class_complete. | T24 |
| [codegen_free](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:255) | Destruye un fragmento temporal y su contenedor; admite NULL. | G05 G10 |
| [codegen_value](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:261) | Construye un fragmento con una referencia a símbolo y su línea de origen. | G02 G05 |
| [codegen_unary](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:269) | Añade un operador unario y línea al fragmento recibido, devolviendo el mismo fragmento modificado. | G05 T34 |
| [codegen_binary](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:276) | Concatena izquierdo y derecho, consume el derecho y agrega la operación; conserva el orden de evaluación. | G05 |
| [codegen_target](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:290) | Acumula un destino de la asignación interna en la lista temporal de destinos. | T18 |
| [codegen_assignment_expression](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:301) | Recorre destinos de derecha a izquierda y emite DUP, destino y asignación, dejando el valor disponible para la expresión exterior. | G05 T18 |
| [codegen_assignment](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:321) | Comprueba códigos en copias directas de objetos y añade destino y asignación al cuerpo; no es un chequeador general INTEGER/SINGLEF. | G05 T24 |
| [codegen_output](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:344) | Añade POUT al valor y lo incorpora al cuerpo actual, consumiendo el fragmento. | G05 G09 |
| [returns_object](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:351) | Simula las categorías objeto/escalar del fragmento para detectar un resultado objeto en RET; no comprueba compatibilidad numérica completa. | G06 |
| [codegen_return](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:389) | Rechaza resultado objeto según la decisión del grupo, agrega RET al valor y lo incorpora al cuerpo activo. | G05 G06 |
| [codegen_function_begin](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:405) | Exige retorno primitivo, crea cuerpo independiente, lo registra para especializar y cambia el contexto conservando el padre. | G05 G06 T22 |
| [codegen_function_abort](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:430) | Restaura el cuerpo de la función exterior o del principal cuando se cierra o descarta una guarda; no libera el cuerpo retenido. | G03 G05 |
| [codegen_function_end](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:441) | Añade RET de cero del tipo declarado para caída al final y restaura el cuerpo exterior. | G05 G06 |
| [append_branch](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:452) | Reserva un destino y emite BF o BI con línea, devolviendo la dirección a completar. | G05 |
| [codegen_if_begin](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:460) | Emite condición y BF con destino pendiente, registrando dónde comienza la condición. | G05 |
| [codegen_if_else](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:470) | Añade BI para saltar el ELSE y completa la salida falsa hacia el inicio de esa rama. | G05 |
| [codegen_if_end](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:477) | Completa el salto pendiente con el primer paso posterior a la selección. | G05 |
| [constant_value](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:481) | Convierte temporalmente el texto de una constante para comparar los límites FROM; no valida los tipos de su cabecera. | T14 |
| [codegen_for_begin](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:496) | Inicializa el control, elige comparación estricta y actualización por los límites y prepara el salto de salida. | G05 T14 |
| [codegen_for_end](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/codegen.c:516) | Emite actualización sobre el control actual, salto a la condición y completa la salida del ciclo. | G05 T14 |

### specialize.c: Evaluación y especialización (41 funciones)

Conviene leer primero valores/entornos, después objetos, después control y llamadas. El presupuesto de ciclos y profundidad protege al compilador; no implementa el control EI del programa generado. El plan separa estas responsabilidades en módulos concretos.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [resize](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:107) | Reserva o redimensiona memoria de evaluación con comprobación de fallo; es infraestructura interna. | T22 |
| [object_location](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:116) | Devuelve una identidad estable para el par objeto padre/campo; crea una ubicación lógica si todavía no existe. | T22 T24 |
| [object_root](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:131) | Sube las ubicaciones de campos hasta el objeto raíz para relacionar escrituras y dependencias. | T22 T24 |
| [type_in_path](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:140) | Indica si un tipo ya pertenece al camino activo de un recorrido recursivo. | T22 T24 |
| [same_value](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:146) | Compara conocimiento y valor numérico/tipo flotante; no implementa una igualdad general de objetos. | T22 |
| [get_value](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:151) | Busca la asociación de símbolo y objeto en el entorno; devuelve valor desconocido si no está registrada. | T22 |
| [set_value](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:159) | Actualiza o agrega una asociación símbolo/objeto/valor en el entorno. | T22 |
| [copy_environment](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:171) | Copia el array de asociaciones del entorno; las identidades de símbolos y capturas siguen prestadas. | T22 |
| [forget_runtime](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:181) | Vuelve desconocidas las asociaciones ordinarias, conservando las declaradas comptime. | T22 |
| [forget_call_writes](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:187) | Invalida conocimiento de variables y objetos que una llamada puede modificar, usando su resumen de escrituras. | G06 T22 |
| [join_environments](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:200) | Combina caminos conservando sólo los valores numéricos conocidos iguales en ambos; el resto pasa a desconocido. | T22 |
| [constant_symbol](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:216) | Comprueba representabilidad, escribe el literal del tipo adecuado e interna la constante; reutiliza su referencia si ya está disponible. | G02 T03 T07 T22 |
| [numeric_constant](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:244) | Interpreta un símbolo INTEGER/SINGLEF como valor conocido y redondea a float cuando corresponde. | T03 T07 T22 |
| [push](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:259) | Añade un valor a la pila temporal de evaluación, aumentando el array. | T22 |
| [pop](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:264) | Retira el último valor; una pila vacía genera diagnóstico de expresión intermedia inválida. | G05 T22 |
| [emit](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:270) | Añade un paso al cuerpo de salida de la transformación. | G05 T22 |
| [discard](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:274) | Elimina un fragmento descartable al final de la salida o emite DROP cuando deben preservarse sus efectos. | T18 T22 |
| [find_template](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:280) | Busca la declaración fuente de una función, normalizando antes una referencia a instancia especializada. | G06 T22 |
| [is_operator](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:287) | Comprueba que un paso sea exactamente el operador solicitado. | G05 T22 |
| [class_has_comptime_path](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:291) | Recorre atributos efectivos y tipos objeto anidados para detectar campos comptime, evitando repetir tipos del camino activo. | T22 T24 |
| [class_has_comptime](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:301) | Inicia la consulta anterior sin camino previo. | T22 T24 |
| [object_field](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:305) | Obtiene el valor de un campo desde una captura o entorno y conserva la identidad/captura de un campo objeto anidado. | T22 T24 |
| [capture_object](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:326) | Crea una captura inmutable de los campos comptime, incluyendo objetos hijos; diagnostica valores desconocidos o una captura recursiva no finita. | G06 T22 T24 |
| [materialize_object](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:370) | Emite MATERIALIZE con la captura obtenida y asocia esa captura al valor temporal. | G05 T22 T24 |
| [has_comptime_effects](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:377) | Examina un rango para detectar escrituras comptime, copias de objetos afectados o llamadas con efectos de compilación. | T14 T22 |
| [apply_effects](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:400) | Invalida conocimiento afectado por una llamada y propaga sus efectos conocidos al entorno del llamador, ajustando el receptor. | G06 T22 |
| [record_return](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:421) | Acumula el valor de retorno; conserva uno conocido sólo si los retornos observados coinciden. | G06 T22 |
| [transform_control](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:427) | Procesa IF o ciclo: selecciona ramas conocidas, expande ciclos con efectos comptime o conserva bifurcaciones, y combina entornos. | G05 T14 T22 |
| [evaluate_binary](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:513) | Calcula conocimiento del resultado si ambos operandos conocidos tienen tipo compatible; emite igualmente la operación runtime y conserva sus límites. | T03 T07 T22 |
| [copy_object_fields](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:550) | Copia conocimiento por posiciones efectivas y campos anidados; verifica determinabilidad y permisos de escrituras comptime, distinguiendo copias de argumentos. | G06 T22 T24 |
| [assign_value](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:589) | Procesa destinos ordinarios, formales y atributos: vincula comptime o emite la escritura runtime y actualiza/invalida el conocimiento correspondiente. | G06 T18 T22 T24 |
| [transform_range](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:647) | Despacha pasos, operandos, atributos, llamadas y operadores de un rango; coordina pila, control, diagnósticos y detención por RET. | G05 G06 T22 |
| [transform](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:831) | Prepara un contexto para un cuerpo, identifica controles, transforma el rango completo y libera pilas y llamadas pendientes. | G05 T22 |
| [matches_environment](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:856) | Decide si una instancia contextual puede reutilizarse comparando receptor, entrada bajo control desconocido y asociaciones comptime. | G06 T22 |
| [instantiate](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:873) | Prepara entorno de activación y formales comptime; reutiliza o crea instancia, transforma su cuerpo y registra efectos, retorno y valores finales. | G06 T22 |
| [specialize_init](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:954) | Reinicia plantillas, instancias, identidades, presupuesto de evaluación, profundidad y cuerpo principal de salida. | G05 T22 |
| [specialize_register](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:967) | Registra una función fuente con referencia prestada a su cuerpo retenido. | G05 G06 T22 |
| [record_runtime_write](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:975) | Añade una escritura no local ordinaria al resumen de efectos de la función, evitando duplicados. | G06 T22 |
| [specialize_program](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:983) | Clasifica dependencias y efectos, los propaga por llamadas, transforma el principal y genera funciones ordinarias no usadas; registra valores finales. | G05 G06 T22 |
| [specialize_print](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:1065) | Imprime principal e instancias ejecutables completas; actualmente no muestra los cuerpos simbólicos de plantillas sin instancia. | G05 G07 T22 |
| [specialize_destroy](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/specialize.c:1075) | Libera ubicaciones lógicas, cuerpos transformados, entornos, instancias, registros de plantillas y diagnósticos retenidos. | G10 T22 |

### grammar.y: Ayudantes de diagnóstico escritos en la gramática (2 funciones)

Las producciones contienen acciones de construcción y recuperación, explicadas en el apartado siguiente. Aquí se listan sólo las dos funciones C definidas manualmente tras el segundo %%.

| Función y definición | Qué hace y qué deja como resultado | Requisitos |
| --- | --- | --- |
| [yyreport_syntax_error](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/grammar.y:529) | Consulta el contexto de Bison y tokens esperados, arma una explicación legible y registra un error pendiente que las producciones pueden especializar. | G03 G07 |
| [friendly_symbol_name](/home/rodrigo/facultad/compi/COMPILADOR5/compiler/src/grammar.y:591) | Traduce nombres internos de tokens a palabras útiles para el diagnóstico, como identifier o integer. | G03 G07 |

## Acciones de la gramática que no son funciones C independientes

`yyparse` es generado por Bison a partir de estas reglas. `$$` lleva el resultado
semántico de la reducción y `$n` el valor de su componente n; `@n` conserva la
ubicación. Los fragmentos y guardas guardados allí tienen destructores para
recuperar memoria y contexto si Bison descarta una construcción incompleta.

| Reglas relacionadas | Responsabilidad y funciones a seguir | Requisitos |
| --- | --- | --- |
| `statements`, `program_name` | Reconocer programa, nombre y delimitadores; declarar PROGRAM mediante scope_program. | G03 G04 |
| `decl_list`, `var_decl`, `id_list` | Reunir identificadores tipados, declararlos y marcar comptime cuando corresponde. | G02 G03 G04 T22 |
| `type`, `primitive_type` | Producir tipo primitivo o resolver un nombre de clase mediante scope_named_type. | G02 G04 T03 T07 |
| `statement`, `compound_stmt`, `simple_stmt`, `single_stmt`, `stmt_block` | Secuenciar sentencias y cuerpos simples o begin/end; cerrar episodios de recuperación. | G03 G07 |
| `assign` | Crear la asignación ordinaria o posicional mediante codegen_assignment o codegen_attribute_assignment. | G05 T29 |
| `expr`, `term`, `factor`, `factor_base`, `nonconstant_factor` | Fijar precedencia y construir fragmentos; sin agrupación aritmética general ni negación de variables. | G03 G05 |
| `constant` | Resolver signo, rechazar 32768 positivo y registrar las constantes negativas válidas. | T03 T07 |
| `assign_chain` y alternativas de `factor` | Reunir destinos internos de = y dejar el valor asignado para el cálculo exterior. | T18 |
| `call`, `call_arguments`, `arg_list` | Resolver función/método, conservar argumentos y receptor, exigir cierre y lista de orden. | G06 T19 T32 |
| `evaluation_order`, `const_list` | Reconocer corchetes y constantes de orden; codegen_call_end realiza la validación semántica. | T19 |
| `cond` | Construir comparación y diagnosticar operandos o comparadores incorrectos. | G03 G05 |
| `if_header`, `if_else_header`, `if_stmt` | Retener direcciones pendientes en la pila del parser y completarlas al reducir ELSE/end_if. | G05 |
| `for_header`, `for_loop` | Retener contexto FROM y emitir inicio/actualización/salida; las reglas auxiliares diagnostican componentes ausentes. | G03 G05 T14 |
| `for_identifier`, `for_assignment`, `for_initial`, `for_to`, `for_final`, `for_by`, `for_step`, `for_repeat` | Reconocer y recuperar la cabecera por partes, antes de analizar su cuerpo. | G03 T14 |
| `function_prefix`, `function_header`, `func_def`, `param_decl_list` | Abrir ámbito y cuerpo, declarar formales en orden, cerrar con retorno por defecto y restaurar contexto. | G03 G04 G05 G06 T22 |
| `method_prefix`, `method_header`, `method_def` | Aplicar las mismas guardas a un método dentro de la clase. | G04 G05 G06 |
| `class_header`, `class_missing_code`, `class_def`, `class_body`, `class_member` | Registrar clase/código y miembros, recuperar errores y finalizar esquema compatible. | G02 G03 T24 |
| `extends_stmt`, `extends_id_list` | Resolver las bases visibles y registrarlas en orden de fuente. | G04 T32 |
| `attr_ref` | Reconocer índice constante o identificador y preparar receptor e índice mediante codegen_attribute. | T29 |
| `pout_stmt` | Construir POUT de cadena o expresión. | G03 G05 T10 |
| `ret_keyword`, `ret_stmt` | Comprobar contexto de función, exigir expresión y construir RET. | G03 G05 G06 |

Las macros `SYNERR` y `RECERR` distinguen un error independiente de un error
pendiente ya contado. `RECOVER` cierra el episodio y restablece Bison;
`RECOVER_STATEMENT` preserva un `;` que necesita una regla exterior. Son mecanismos
específicos del parser, no una base para macros semánticas generales.

## Funciones propuestas para completar el compilador

**Ninguna de las funciones de este apartado está implementada todavía.** Los
nombres expresan contratos mínimos de la arquitectura. No se crearán wrappers
que sólo renombren otra llamada: al extraer un módulo, las funciones equivalentes
existentes se trasladarán o renombrarán, evitando mantener dos autoridades para
la misma regla. Los ayudantes que surjan al implementar se incorporarán a este
informe en ese mismo cambio.

### Coordinación, programa e integridad

| Función propuesta | Entrada, resultado y límites | Requisitos |
| --- | --- | --- |
| `compiler_init` | Inicializar un compiler_t concreto, TS y contenedores vacíos. El contexto será dueño de los recursos de la compilación. | G02 G10 L01 |
| `compiler_compile` | Recibir archivo y opciones; ejecutar las etapas habilitadas en orden y devolver éxito/fallo. El modo TP3 conserva sus límites; el completo incluye tipos y destino. | G03 G08 G09 |
| `compiler_destroy` | Liberar contextos, cuerpos y por último TS y entrada, también después de errores. | G10 L01 |
| `ir_add_control` | Registrar región IF/FROM, límites y cabecera junto con la Polaca. Las direcciones se reubican cuando se concatenan fragmentos. | G05 T14 |
| `ir_validate_program` | Revisar cuerpos, saltos, regiones, correspondencia de llamadas y equilibrio de operandos; devolver fallos internos con origen, sin ejecutar el programa. | G05 G07 |
| `ir_print_templates` | Recibir cuerpos fuente y mostrar los contextuales como plantillas simbólicas numeradas; no sustituir parámetros por valores inventados. | G05 G07 T22 |

Las estructuras internas de las especializaciones deben poder recorrerse mediante
el contenedor de programa. El emisor recibe referencias C, nunca vuelve a leer
la salida textual de `rpn_print`.

### Clases y tipos

| Función propuesta | Entrada, resultado y límites | Requisitos |
| --- | --- | --- |
| `class_build_schema` | Trasladar scope_class_layout a classes.c; recibir clase y producir atributos efectivos sin offsets de máquina. | T24 T29 T32 |
| `class_find_member` | Unificar la búsqueda de método/atributo con categoría explícita; devolver declaración, ausencia o ambigüedad. | G04 T32 |
| `class_check_code` | Trasladar la finalización y comparación de clases de igual código; no mezclarla con emisión de copias. | T24 |
| `class_attribute_type` | Recibir esquema e índice conocido o variable; devolver el tipo verificable y la información necesaria para selección. Aplicar el contrato heterogéneo que se acuerde. | T29 G08 |
| `type_of_symbol` | Devolver type_t con enum y referencia de clase a partir de la declaración o categoría literal; no deducir el tipo por el nombre calificado. | G02 G08 |
| `type_binary_result` | Recibir operador y tipos izquierdo/derecho; devolver tipo de resultado o TYPE_ERROR, sin insertar conversiones implícitas. | G08 T03 T07 |
| `type_assignment_allowed` | Comparar tipos de origen/destino, incluidos formales y atributos; para objetos delegar compatibilidad y código en classes. | G08 T24 |
| `type_check_body` | Recorrer un cuerpo fuente con pila de tipos y regiones; comprobar todos sus pasos, TOI, RET, ARG y cabeceras FROM, aunque una rama no se ejecute. | G08 T14 T18 T19 T34 |

Los predicados devuelven datos y no imprimen por su cuenta salvo que su contrato
incluya diagnóstico con ubicación. El recorrido que conoce la línea emite el
mensaje de combinación incompatible y continúa con TYPE_ERROR para no provocar
una cascada artificial. Un error no convierte el resto de la expresión en INTEGER.

### Valores y objetos de compilación

| Función propuesta | Entrada, resultado y límites | Requisitos |
| --- | --- | --- |
| `ct_value_from_symbol` | Extraer numeric_constant: leer un símbolo numérico como valor conocido con el tipo y redondeo del lenguaje. No crear otro símbolo ni emitir pasos. | T03 T07 T22 |
| `ct_constant_symbol` | Extraer constant_symbol: validar representabilidad y registrar el literal correspondiente a un valor ya conocido. Devolver una referencia prestada de TS. | G02 T03 T07 T22 |
| `ct_evaluate_binary` | Recibir operador y valores; devolver conocido/desconocido o error de evaluación, aplicando INTEGER/SINGLEF sin emitir destino. | T03 T07 T22 |
| `ct_convert_integer` | Convertir un valor requerido en compilación según la política EG acordada; distinguir desconocido, conversión inválida y valor correcto. | T22 T34 EG |
| `ct_env_get` | Obtener una asociación por símbolo y objeto; ausencia significa desconocido. Extraer get_value sin cambiar esa regla. | T22 |
| `ct_env_set` | Crear o actualizar una asociación, dejando propiedad de identidades en sus dueños originales. Extraer set_value. | T22 |
| `ct_env_merge` | Unir conocimiento al salir de ramas conservando sólo lo demostrable en ambos caminos; extraer join_environments. | T22 |
| `ct_object_capture` | Extraer capture_object y ayudantes; crear captura inmutable por campos originales y rutas, con propietario explícito. | G06 T22 T24 |
| `ct_object_copy` | Extraer copy_object_fields; copiar por posición y separar identidad del destino de la captura del origen. | G06 T22 T24 |
| `ct_object_materialize` | Registrar la asociación entre valor runtime y captura, conservando el punto exacto de evaluación. | T19 T22 T24 |

Estos módulos no introducen otro lenguaje ni un intérprete del programa final.
Su trabajo es determinar constantes y preparar los cuerpos que el backend ejecutará.

### Descomposición del recorrido de especialización

| Función propuesta | Entrada, resultado y límites | Requisitos |
| --- | --- | --- |
| `specialize_symbol` | Procesar un operando: consultar entorno, sustituir comptime y capturar objetos cuando corresponda; devolver valor y pasos resultantes. | G05 T22 |
| `specialize_attribute` | Resolver receptor e índice y consultar el valor del campo; conservar índices runtime admitidos por el contrato. | T22 T29 T32 |
| `specialize_assignment` | Separar las escrituras comptime de las ordinarias y validar control conocido; delegar la copia de campos. | T18 T22 T24 |
| `specialize_call` | Preparar argumentos, elegir instancia, conservar efectos y restaurar la llamada pendiente; no evaluar POUT ni imponer EI como límite de profundidad. | G06 T19 T22 |
| `specialize_control` | Procesar regiones explícitas IF/FROM en lugar de reconstruir su significado por patrones de saltos. | G05 T14 T22 |
| `specialize_body` | Coordinar los casos anteriores con contexto local, pila y origen, sin contener toda su implementación en un switch de cientos de líneas. | G05 T22 L01 |

Las funciones instantiate, matches_environment y los resúmenes de efectos se
conservan inicialmente. No se elimina su caché sin medir y probar equivalencia;
tampoco se añade otra capa de caché para compensar una separación deficiente.

### Distribución de datos

| Función propuesta | Entrada, resultado y límites | Requisitos |
| --- | --- | --- |
| `layout_class` | Calcular tamaño, alineación y ubicaciones runtime por identidad de atributo efectivo; excluir celdas comptime y detectar materializaciones sin tamaño finito. | G09 T24 T29 T32 |
| `layout_function` | Calcular marco con enlace léxico, parámetros, receptor, locales y auxiliares; mantenerlo separado de la identidad de los símbolos. | G06 G09 |
| `layout_address` | Recibir símbolo, función activa y receptor; devolver ubicación y recorrido de padres léxicos o campo efectivo que debe emitir el backend. | G04 G09 T29 T32 |

Los offsets son un resultado del destino elegido. Clases y ámbitos no contienen
instrucciones WAT/Pentium. Los índices posicionales siguen contando campos
efectivos aunque un campo comptime no tenga almacenamiento ordinario.

### Emisión WebAssembly propuesta

Si el grupo confirma Pentium, este bloque se sustituye por funciones equivalentes
de ese emisor; el resto del plan conserva sus contratos. No se implementarán los
dos destinos por anticipado.

| Función propuesta | Entrada, resultado y límites | Requisitos |
| --- | --- | --- |
| `wasm_emit_program` | Recibir programa ya validado y layouts; escribir módulo, datos, imports y cuerpos en un archivo temporal, y finalizarlo sólo si no falló. | G07 G09 |
| `wasm_emit_body` | Recorrer un cuerpo especializado y sus regiones; mantener pila de referencias a auxiliares y destinos, no ejecutar valores del programa. | G05 G09 |
| `wasm_emit_operator` | Consumir operandos ya calculados y escribir operación y auxiliar tipados; para operaciones controladas delegar el chequeo correspondiente. | G08 G09 EC ED EG |
| `wasm_emit_assignment` | Almacenar un valor ya capturado en su destino sin reevaluar la expresión; DUP puede referenciar el mismo auxiliar. | G09 T18 |
| `wasm_emit_if` | Emitir if/else estructurado para una región validada, manteniendo una pila de operandos coherente al salir de cada rama. | G05 G09 |
| `wasm_emit_from` | Emitir inicialización, block/loop, comparación y actualización posterior; conservar la variable modificable por el cuerpo. | G09 T14 |
| `wasm_emit_call` | Administrar marco pendiente, copia ARG en el orden ya fijado y CALL al destino concreto con receptor y enlace léxico. | G06 G09 T19 |
| `wasm_emit_return` | Copiar el resultado al retorno del cuerpo y dirigir toda salida al epílogo que libera marco y limpia la activación. | G06 G09 EI |
| `wasm_emit_object_copy` | Copiar posiciones compatibles con layouts de origen y destino; realizar copia de valor independiente también en campos anidados. | G06 G09 T24 |
| `wasm_emit_attribute` | Emitir dirección de campo por identidad efectiva o selección por índice variable, incluidos controles que requiera su contrato. | G09 T29 T32 |
| `wasm_emit_materialize` | Materializar las constantes capturadas junto a los campos ordinarios del valor, sin releer el objeto después de argumentos posteriores. | G09 T19 T22 T24 |
| `wasm_emit_output` | Elegir la importación de salida por tipo o cadena, preservando contenido y orden de evaluación. | G09 T10 |
| `wasm_check_float_add` | Emitir la condición de overflow SINGLEF y la llamada de error; no usar el rango f64 como límite del lenguaje. | EC |
| `wasm_check_integer_product` | Comprobar producto ampliado contra el rango int16 antes de almacenarlo. | ED T03 |
| `wasm_check_float_conversion` | Comprobar finitud, rango y política de fracción antes de la instrucción de conversión que pueda generar un trap genérico. | EG T34 |
| `wasm_function_entry` | Emitir guardia y activación por función fuente al entrar al cuerpo, después de los efectos de argumentos. | EI |
| `wasm_function_exit` | Emitir limpieza de activación y recursos en el epílogo compartido por RET explícitos y retorno por defecto. | G06 EI |

Cada auxiliar lleva nombre estable, tipo y paso fuente para poder seguirlo en
el archivo destino. El emisor no vuelve a comparar cadenas "INTEGER"/"SINGLEF"
en cada operación: recibe tipos normalizados y ubicaciones calculadas.

### Soporte de ejecución propuesto

| Función propuesta del runner | Contrato | Requisitos |
| --- | --- | --- |
| `printInteger` | Recibir el valor INTEGER producido por el módulo y escribirlo con el formato acordado. | G09 T03 |
| `printFloat` | Mostrar el valor SINGLEF recibido, sin efectuar las operaciones del lenguaje en JavaScript. | G09 T07 |
| `printString` | Leer bytes y longitud de la memoria del módulo y mostrarlos; no usar la salida del compilador como programa. | G09 T10 |
| `runtimeFail` | Recibir código de error y línea; mostrar mensaje específico e interrumpir la ejecución completa con estado de fallo. | G07 EC ED EG EI |
| `run` | Cargar el .wasm real, enlazar imports, invocar el punto de entrada una vez y comunicar éxito o fallo. | G09 G10 |

Las condiciones cdgi están dentro del programa emitido. El runner sólo presenta
el error y termina. No basta llamar a print y continuar, ni interpretar la
Polaca con JavaScript como sustituto de generar WebAssembly.

## Ejemplos para seguir las funciones en orden

### Una asignación con valor interno

Para `a := b=2$i + c;`, seguir yylex → sa_identifier/sa_int_const → reglas
assign_chain/factor/expr → codegen_target → codegen_assignment_expression →
codegen_binary → codegen_assignment. La Polaca debe escribir 2 en b y mantener
ese mismo 2 como operando de la suma. El traductor futuro usará un auxiliar para
el valor y otro para el resultado; no volverá a leer b después de sus efectos.

### Una llamada con orden cambiado

Para `f(a,b=4$i,a+b)[1$i,3$i,2$i]`, la gramática conserva tres fragmentos.
codegen_call_end emite primero el argumento 1 y su copia al formal 1, luego el
argumento 3 y su copia al formal 3, y al final el argumento 2 y su copia al formal
2. specialize_call conservará ese orden. wasm_emit_call preparará marcos
pendientes diferentes para las llamadas internas sin sobrescribir otro formal.

### Un valor de compilación

Para `comptime integer x;` y luego `x := 3$i; x := x+1$i; pout(x);`, las
declaraciones marcan el símbolo. assign_value registra valores 3 y 4 en cada
punto; transform_range sustituye la lectura de x por la constante 4. El POUT
permanece en el cuerpo ejecutable. No se ejecuta dentro del compilador.

### Un error de producto

Para un producto INTEGER que resulta 40000, type_check_body aceptará los tipos
de ambos operandos; wasm_emit_operator emitirá el producto y
wasm_check_integer_product verificará el rango −32768..32767. El programa
llamará runtimeFail y terminará antes de una salida posterior. No es un error
de incompatibilidad de tipos, ni overflow de entero de 32 bits.

## Herramientas de construcción y pruebas

- `build.sh` genera el parser sin conflictos y compila los módulos con warnings
  tratados como errores. Su lista de fuentes deberá actualizarse al extraer módulos.
- `run_tests.sh` construye y ejecuta ambos verificadores; `--show` es inspección
  manual y no sustituye sus comprobaciones automáticas.
- `tests/verify_tp3.py` contiene comprobaciones de compilación y un evaluador
  independiente de la IR para los casos actuales de TP3.
- `tests/tp3_course_cases.py` declara escenarios con referencias y expectativas;
  `verify_tp3_course.py` los materializa y comprueba en dos modos.
- Se propone un verificador específico TP4 que compile fuente a destino, lo
  procese, ejecute el .wasm y compare salida, mensajes y estado. El evaluador de
  IR no sustituye esa ejecución ni debe convertirse en el backend.

La compilación aislada de esta revisión pasó 107 comprobaciones y 340 ejecuciones
de 170 escenarios. No se afirma cobertura completa de objetos anidados ni TP4.
Los escenarios de tipos diferidos, recursión y truncamiento fraccionario deben
tener expectativas propias al validar la etapa final.

## Cómo mantener la explicación alineada con el código

Al añadir o modificar una función, su ficha debe indicar entradas, resultado,
efectos sobre estado, propiedad de memoria, errores posibles, requisito y una
prueba observable cuando corresponda. Si cambia de módulo, actualizar su enlace.
Los algoritmos centrales —control, copia ordenada, especialización y cdgi— deben
tener además un ejemplo completo fuente → Polaca → destino → resultado.

La matriz y este inventario se verifican contra las definiciones reales: una
declaración en un header no demuestra implementación y una prueba que sólo busca
un nombre en un archivo no acredita comportamiento. El informe formal de entrega
debe incluir también grupo, integrantes, correos, fuentes de IA disponibles,
decisiones y conclusiones requeridas por la cátedra.

