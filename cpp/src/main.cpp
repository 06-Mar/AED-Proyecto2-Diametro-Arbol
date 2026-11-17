#include "json_export.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <climits>
#include <filesystem>

namespace {
void help() {
    std::puts("Uso: diametro --input archivo.txt --output archivo.json [--start 1]\n"
              "Arbol no dirigido y no ponderado; IDs de 1 a n. Se ignoran lineas vacias.");
}
int parseStart(const char* text) {
    if (!*text) throw std::runtime_error("--start requiere un entero positivo.");
    for (const char* p = text; *p; ++p)
        if (*p < '0' || *p > '9') throw std::runtime_error("--start requiere un entero positivo.");
    errno = 0;
    char* end = nullptr;
    long value = std::strtol(text, &end, 10);
    if (errno || *end || value < 1 || value > INT_MAX)
        throw std::runtime_error("--start fuera del rango admitido.");
    return static_cast<int>(value);
}
}

int main(int argc, char** argv) {
    try {
        const char* input = nullptr;
        const char* output = nullptr;
        int start = 1;
        bool hasStart = false;
        for (int i = 1; i < argc; ++i) {
            const char* arg = argv[i];
            if (std::strcmp(arg, "--help") == 0 && argc == 2) { help(); return 0; }
            if (std::strcmp(arg, "--input") != 0 && std::strcmp(arg, "--output") != 0 && std::strcmp(arg, "--start") != 0)
                throw std::runtime_error("Opcion desconocida. Consulte --help.");
            if (++i == argc || !*argv[i] || std::strncmp(argv[i], "--", 2) == 0)
                throw std::runtime_error("Falta el valor de una opcion.");
            if (std::strcmp(arg, "--input") == 0) {
                if (input) throw std::runtime_error("--input repetido.");
                input = argv[i];
            } else if (std::strcmp(arg, "--output") == 0) {
                if (output) throw std::runtime_error("--output repetido.");
                output = argv[i];
            } else {
                if (hasStart) throw std::runtime_error("--start repetido.");
                start = parseStart(argv[i]); hasStart = true;
            }
        }
        if (!input || !output) throw std::runtime_error("--input y --output son obligatorios. Consulte --help.");
        // Tambien protege alias, enlaces y rutas relativas que apuntan a la entrada.
        std::error_code error;
        if (std::filesystem::equivalent(input, output, error))
            throw std::runtime_error("La salida no puede sobrescribir el archivo de entrada.");
        Tree tree = readTree(input);
        validateTree(tree, start);
        Trace trace(tree.n);
        BFSResult first = bfsWithTrace(tree, start, 1, trace);
        BFSResult second = bfsWithTrace(tree, first.farthestNode, 2, trace);
        DiameterResult diameter = calculateDiameter(tree, first, second, trace);
        exportJson(output, tree, first, second, diameter, trace);
        std::printf("n=%d | extremos=[%d,%d] | diametro=%d aristas\nJSON generado: %s\n",
                    tree.n, diameter.a, diameter.b, diameter.length, output);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
}
