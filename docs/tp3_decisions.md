# Definiciones confirmadas para completar TP3

> Implementation update: the confirmed rules are implemented; see [tp3_implementation.md](tp3_implementation.md) for the current behavior. Neither group member belongs to the 2011 study plan, so Simple Reduction does not apply. The original plan and decisions below are retained as design history.

Fecha: 23 de octubre de 2026.

Estas definiciones fueron indicadas por el usuario después del análisis de los
puntos particulares. Son criterios acordados para la siguiente implementación;
no se presentan como citas textuales de la cátedra ni como funcionalidades ya
implementadas. La documentación del estado del código sigue en
[tp3_implementation.md](tp3_implementation.md).

## Decisiones confirmadas

1. **Comptime bajo control desconocido.** Rechazar una modificación de una
   variable comptime bajo una condición cuyo resultado no sea conocido en
   compilación. El diagnóstico corresponde a la asignación dentro de la rama;
   no se espera hasta un uso posterior. No se aceptará la modificación por el
   solo hecho de que ambas ramas dejen el mismo valor. El requisito general de
   determinar valores y sustituir usos está en TP3 p.8; la elección entre las
   políticas de diagnóstico fue confirmada por el usuario.
2. **Compatibilidad de estructuras.** Comparar atributos por igualdad de tipos
   y en el mismo orden. Los nombres de los atributos y los métodos no integran
   ese criterio. El código de clase continúa siendo necesario para asignaciones
   entre objetos de clases distintas, conforme al tema 24 de TP3 p.9.
3. **Orden posicional.** Ubicar los atributos heredados antes de los propios.
   El acceso comienza en cero, como muestra TP2 p.7. La regla heredados/propios
   es la definición confirmada por el usuario; el material no la especificaba.
   No se atribuye a esta respuesta una elección adicional del recorrido entre
   varias bases: se debe conservar y documentar expresamente el criterio de
   recorrido usado al concretar la numeración.
4. **Diamante.** Usar identidad por declaración. Encontrar el mismo símbolo a
   través de varios caminos no constituye por sí solo una ambigüedad. Dos
   declaraciones distintas homónimas siguen sujetas al error del tema 32 de
   TP3 p.11. No se debe confundir esta decisión de identidad con una regla nueva
   de selección de la primera base.
5. **Orden de argumentos.** Todos los argumentos deben aparecer exactamente
   una vez. La lista debe representar un orden completo de las posiciones de
   los argumentos; no se admiten repetidos ni posiciones que no correspondan a
   un argumento. Se mantiene la lista obligatoria del tema 19 y la correspondencia
   de cada argumento con su formal. TP2 p.4 y TP3 pp.5 y 7 fundamentan el orden,
   las cantidades y la copia; la exigencia de aparición única queda confirmada.
6. **Retornos.** El usuario confirmó que no se admiten retornos de objetos.
   Los tipos devueltos por funciones y métodos serán INTEGER o SINGLEF. Esto no
   elimina las declaraciones, parámetros ni asignaciones de objetos de los
   temas 24, 29 y 32.

## Verificación de retornos de objetos

Se revisaron las filminas disponibles en compi: UNC.01, UNC.02, 03, 04, 05,
07.1, 08.0 a 08.4, Presentación TP1 y Portabilidad 2026. No se encontró una
regla o un ejemplo del lenguaje del trabajo práctico que habilite funciones
que devuelvan objetos. Los ejemplos de otros lenguajes o de código objeto no
definen el tipo de retorno admitido por este lenguaje.

Referencias específicas:

- **TP2 p.1:** presenta los tipos primitivos, la forma `<tipo> FUNCTION ...` y
  el retorno como expresión aritmética. Para el grupo, los tipos primitivos
  asignados son INTEGER y SINGLEF.
- **TP2 p.2:** incorpora declaraciones de clases y objetos, pero no muestra ni
  define una función cuyo tipo de retorno sea una clase.
- **TP3 p.5:** exige registrar el tipo devuelto y un retorno por defecto del
  tipo declarado, sin añadir una regla para retornos de clases ni su valor por
  defecto.

La conclusión es de alcance: **el material consultado no aporta respaldo
explícito para implementar retornos de objetos**. Se conservan los retornos
primitivos existentes. La ausencia de una autorización explícita no se
presenta como una prohibición textual de la cátedra. Que la producción `type`
de nuestra gramática acepte nombres de clases tampoco prueba ese requisito.
Después de esta verificación, el usuario fijó expresamente la decisión 6.

## Estado del código al registrar estas decisiones

Este documento no modifica el generador ni sus diagnósticos. El código todavía
marca como pendientes la evaluación comptime, la compatibilidad estructural,
la numeración heredada y los caminos repetidos de herencia. Los órdenes que no
son permutaciones ya se detectan, pero conservan el diagnóstico anterior de
aclaración pendiente. Esas implementaciones y sus pruebas deben ajustarse a las
definiciones confirmadas arriba.

El plan para concretarlas está en [tp3_completion_plan.md](tp3_completion_plan.md).
