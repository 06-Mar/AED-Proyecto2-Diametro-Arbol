# AED-Proyecto2-Diametro-Arbol

Implementación en **C++17** del diámetro de un árbol no dirigido y no ponderado mediante **dos BFS**, con exportación de los eventos reales a JSON `1.0` para animación en Manim u otra herramienta.

Proyecto Final 2 de Algoritmos y Estructuras de Datos, tema 12. El contrato completo está en [docs/REQUERIMIENTOS_CPP_DIAMETRO_ARBOL.md](docs/REQUERIMIENTOS_CPP_DIAMETRO_ARBOL.md).

## Compilar

Requiere un compilador C++17 con soporte de `std::filesystem` (GCC 9+, Clang con biblioteca equivalente o Visual Studio 2019+) y CMake 3.16+. Python 3 es opcional y se utiliza únicamente para las pruebas. No se descargan dependencias.

```sh
cmake -S cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

En Windows con MinGW, seleccionar el generador al configurar:

```powershell
cmake -S cpp -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

También se puede compilar directamente desde la raíz:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icpp/include cpp/src/main.cpp cpp/src/tree.cpp cpp/src/bfs.cpp cpp/src/diameter.cpp cpp/src/json_export.cpp -o diametro
```

## Ejecutar y generar el JSON

Desde la raíz del repositorio, en Linux/macOS:

```sh
./build/diametro --input inputs/arbol_demo.txt --output outputs/animacion_diametro.json --start 1
```

En PowerShell con MinGW:

```powershell
.\build\diametro.exe --input inputs/arbol_demo.txt --output outputs/animacion_diametro.json --start 1
.\build\diametro.exe --input inputs/un_nodo.txt --output outputs/animacion_un_nodo.json
```

Con el generador de Visual Studio, el ejecutable queda en `build/Release/diametro.exe`.

`--input` y `--output` son obligatorios; `--start` vale `1` por defecto. `--help` muestra la ayuda. La carpeta de salida debe existir. Los errores se escriben en `stderr` y devuelven código `1`; el éxito devuelve `0` e imprime nodos, extremos, longitud y salida.

El ejemplo de siete nodos produce extremos **[7, 4]**, longitud **5**, camino **[7, 6, 5, 2, 3, 4]** y **86 eventos**. Los archivos de [outputs/](outputs/) fueron generados ejecutando el código C++; pueden entregarse directamente al equipo de animación.

## Entrada

```text
7
1 2
2 3
3 4
2 5
5 6
6 7
```

Cada línea no vacía contiene un registro: primero `n >= 1`, después exactamente `n-1` pares. Se ignoran líneas vacías, espacios, tabulaciones y finales LF/CRLF. Se rechazan comentarios, pesos, tokens adicionales, pares partidos entre líneas, IDs fuera de `1..n`, lazos, aristas duplicadas en cualquiera de sus orientaciones y grafos desconectados. Los enteros deben estar dentro de `[-2147483647, 2147483647]`; el número de nodos se limita a `INT_MAX/2` por los índices de las dos orientaciones, y al espacio disponible en memoria.

## Estructuras implementadas manualmente

- `Array<T>`: arreglo de tamaño fijo con `new[]`/`delete[]`, propiedad exclusiva y movimiento; libera memoria al salir de ámbito incluso ante errores.
- `Tree`: listas de adyacencia enlazadas en arreglos propios, con cabeza y cola por nodo. Insertar al final conserva el orden del archivo en tiempo constante.
- `Queue`: cola circular FIFO con arreglo propio, sin `std::queue`.
- `Trace`: arreglo propio de eventos tipados; registra cada operación durante la ejecución, sin guardar copias completas del estado.
- BFS, distancias, padres, desempate por menor ID y reconstrucción del camino: lógica propia e iterativa.
- Serialización JSON manual con funciones de E/S estándar; los textos serializados son constantes ASCII y los valores variables son enteros. Las rutas nunca se incluyen en el JSON.

No se usan contenedores ni algoritmos STL (`vector`, `queue`, `list`, `map`, `set`, `sort`, etc.) ni bibliotecas JSON externas. Se usa la biblioteca estándar para E/S, excepciones y `std::filesystem::equivalent`, que evita sobrescribir la entrada mediante alias de ruta. En Windows se usa además `MoveFileExA` para reemplazar la salida terminada. Las rutas de la CLI utilizan la codificación nativa de las funciones de archivos de C; para máxima portabilidad en Windows, usar nombres de ruta ASCII. El contenido JSON siempre es ASCII y, por tanto, UTF-8 válido.

## Algoritmo y costo

1. Leer las aristas y construir ambas orientaciones de la adyacencia.
2. Detectar duplicados mediante marcas por origen: al recorrer los vecinos de `u`, marcar cada vecino con `u`. Una marca repetida identifica una arista duplicada. No requiere ordenar, hashing ni comparar cada arista con todas las anteriores.
3. Verificar conectividad con una cola propia. Este recorrido de validación no forma parte de las dos fases animadas.
4. BFS 1 desde `--start`: encontrar el nodo más lejano `a`.
5. BFS 2 desde `a`: encontrar el nodo más lejano `b`; su distancia es el diámetro.
6. Seguir los padres del segundo BFS desde `b` y llenar el camino en orden inverso, entregándolo desde `a` hasta `b`.

En un árbol existe un único camino entre cada par de nodos; el primer recorrido alcanza un extremo de algún diámetro y el segundo obtiene su longitud máxima. Los empates se resuelven comparando IDs durante el recorrido, sin alterar el orden de vecinos.

Cada BFS descubre, encola, desencola y termina cada nodo una vez; examina cada arista dos veces. Emite `2 + 4n + 2(n-1) = 6n` eventos. Ambos BFS y los dos eventos de resultado suman **`12n + 2`**. Esta capacidad se reserva una sola vez y el contador refleja las inserciones reales.

La lectura estructural, validación, ambos BFS, comprobación del camino y registro de eventos requieren **O(n) tiempo y O(n) memoria**, en el modelo usual de operaciones sobre enteros. El JSON contiene O(n) registros; su tamaño en bytes también incluye la cantidad de dígitos de los IDs y distancias, y el costo de lectura/escritura es proporcional al texto procesado. No se usa recursión ni snapshots de la cola.

## Entrega al animador

Leer `schema_version == "1.0"` y reproducir `events` en orden de `step`:

| Evento | Acción |
|---|---|
| `phase_start` | Reiniciar cola y estilos de visita; mantener el árbol. |
| `node_discovered` | Mostrar distancia y padre; marcar descubierto. |
| `node_enqueued` / `node_dequeued` | Añadir al final / retirar del frente de la cola visual. |
| `edge_examined` | Iluminar `edge_id`; distinguir `new` y `already_seen`. |
| `node_finished` | Marcar el nodo como terminado. |
| `phase_end` | Resaltar `farthest_node` y su distancia. |
| `diameter_path` | Resaltar `nodes` y `edges` en el orden recibido. |
| `diameter_result` | Mostrar extremos y longitud final. |

El animador decide coordenadas y duración de cada transición. No necesita recalcular recorridos ni distancias. `runs` conserva los resultados de ambos BFS ordenados por ID y `diameter` incluye el camino final. Los IDs de arista siempre son `min-max`, incluso cuando se recorren en el sentido opuesto.

## Integridad de salida

Primero se valida toda la entrada y el resultado. Se escribe un temporal exclusivo junto al destino; se comprueban escritura, vaciado y cierre antes de renombrarlo. Ante fallos se limpia el temporal propio y se conserva la salida anterior. Un temporal preexistente no se sobrescribe. No se imprime éxito si el reemplazo falla. Una interrupción abrupta puede dejar un temporal que se puede eliminar manualmente; nunca se presenta como el JSON final.

## Pruebas

```sh
ctest --test-dir build -C Release --output-on-failure
```

O directamente, con la ruta del ejecutable:

```sh
python tests/test_diametro.py build/diametro
```

La batería cubre T1–T12 del documento, sintaxis estricta, opciones de CLI, protección de entrada, reemplazo seguro, colisiones de temporales y determinismo. Compara **todo el JSON y cada evento** con un oráculo independiente en Python; en 75 árboles aleatorios con semilla fija también verifica el diámetro mediante distancias entre todas las parejas. Prueba una cadena y una estrella de 10 000 nodos para verificar el uso iterativo y la validación de duplicados. La prueba de destino de solo lectura es específica de Windows y se omite en otros sistemas. GitHub Actions está configurado para ejecutar la batería en Windows y Linux.

## Archivos

```text
cpp/include/    Arreglos, cola, árbol, eventos y resultados
cpp/src/        Lectura, validación, BFS, diámetro, JSON y CLI
inputs/         Árbol demo, un nodo, dos nodos, cadena y estrella
outputs/        JSON completos generados por C++ para animación
tests/          Oráculo y pruebas de caja negra
docs/           Requerimientos originales del equipo
```
