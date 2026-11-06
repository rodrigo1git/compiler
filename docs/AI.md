# Intervenciones de IA

Se utilizó Codex mediante chat y herramientas locales para revisar el proyecto,
refactorizar responsabilidades y agregar el backend WebAssembly y su validación.
En esta etapa participó el modelo GPT-6.1-sol. También hubo intervenciones previas
registradas en la conversación; no se dispone aquí de un registro exportado
completo para atribuirles modelos o prompts adicionales con exactitud.

La instrucción que autorizó esta implementación fue:

> hacelo. hace testing. verifica a que funcione todo correctamente. primero que todo decime que temas nos toco a nuestro grupo y defini correctamente la consigna y despues empeza a implementar

Antes se proporcionaron PLAN_FINALIZACION_COMPILADOR.md e
INFORME_FUNCIONES_COMPILADOR.md y se pidió su crítica. Sus contratos y las reglas
de TP1–TP4 sirvieron de entrada. La implementación se hizo sobre la rama
feat/tp4-webassembly, partiendo del commit f1ad28d.

Partes agregadas o modificadas con asistencia de IA: chequeo de tipos, política
numérica TP4, distribución de memoria, emisor WAT, imports/runner/ensamblado,
metadatos y consultas de IR, captura de índices, lecturas variables comptime,
pruebas y documentación. La refactorización anterior también fue asistida.

Los integrantes deben revisar y poder justificar el desarrollo completo, como
pide TP4. Este documento no sustituye sus datos personales, sus conclusiones ni
el registro histórico de prompts que conserven por otros medios.


La revisión posterior fue solicitada con «solucionalo», acompañada de dos
contraejemplos: incompatibilidades dependientes de comptime eliminadas por
especialización y expansión ilimitada de llamadas recursivas ordinarias. Codex
reprodujo los casos, agregó auditoría de rangos descartados, enlazó reentradas a
la instancia activa, incorporó regresiones y completó el inventario individual.
Se conservaron los cambios de nombres existentes en el directorio de trabajo.

La instrucción posterior «hace la ultima revision manual» produjo tres
contraejemplos adicionales del chequeo de código descartado: pérdida del resultado
conocido de una llamada, invalidación de un índice por una escritura ajena y
lectura de un campo viejo después de una copia de objeto. Se corrigieron usando
la evaluación de llamadas existente y capturas aisladas para las copias, y se
agregaron casos válidos e inválidos permanentes. La comprobación manual también
contrastó los diagnósticos, el WAT y las salidas reales del módulo.

La instrucción «soluciona lo que sabes que esta mal y lo otro hace un informe
aca en el chat» autorizó corregir dos fallos de una auditoría posterior: índices
comptime pertenecientes al receptor de un método e incompatibilidades que recién
aparecían en iteraciones posteriores de ciclos descartados. Se unificó la lectura
de símbolos, se amplió el recorrido aislado de esos ciclos y se incorporaron
regresiones. También se corrigió el verificador TP4 para exigir diagnósticos
concretos y rechazar errores de sanitizadores. Se comprobaron las suites normales,
sanitizadores, navegador y resultados manuales registrados en VALIDATION.md.


La instrucción «ignora este error y arregla los otros» excluyó la validación de
plantillas sin invocar y autorizó corregir H1–H3 de la auditoría del 6 de noviembre:
receptores de objetos anidados, igualdad interna de ceros SINGLEF y llamadas con
argumentos comptime dentro de FROM. Codex incorporó 38 regresiones permanentes,
resolución común de receptores y un análisis previo de requisitos por iteración
sobre entornos privados, sin instanciación ni diagnósticos especulativos. Las
expectativas se comprobaron ejecutando Wasm real, incluida salida textual para
infinitos. Se conservó un cambio concurrente de captura de índices en codegen.c
y se completó su integración retirando referencias a una función eliminada;
el evaluador TP3 se actualizó para reconocer esa IR. La validación de esta
intervención se registra separadamente en CORRECTIONS.md.
