#!/bin/bash

echo "=================================================="
echo "       CONJUNTO DE PRUEBAS DEL COMPILADOR          "
echo "=================================================="
echo ""

for file in tests/*.txt; do
    echo "--------------------------------------------------"
    echo "Ejecutando: $file"
    echo "Contenido de la prueba y error esperado:"
    cat "$file"
    echo ""
    echo "--- SALIDA DEL COMPILADOR ---"
    ./compiler "$file"
    echo ""
done
