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
