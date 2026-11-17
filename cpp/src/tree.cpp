#include "tree.hpp"
#include <cstdio>
#include <climits>

namespace {
struct File {
    std::FILE* handle;
    explicit File(const char* path) : handle(std::fopen(path, "rb")) {
        if (!handle) throw std::runtime_error("No se pudo abrir el archivo de entrada.");
    }
    ~File() { std::fclose(handle); }
};

// Lee por lineas sin limite artificial de longitud ni almacenamiento del texto.
// Se ignoran lineas vacias; cada linea no vacia admite uno o dos enteros.
bool readRecord(std::FILE* file, int& a, int& b, int& count) {
    for (;;) {
        count = 0;
        bool token = false, digit = false, negative = false, any = false;
        unsigned long long value = 0;
        for (;;) {
            int c = std::fgetc(file);
            if (c == EOF && std::ferror(file)) throw std::runtime_error("Fallo al leer la entrada.");
            const bool separator = c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == EOF;
            if (separator) {
                if (token) {
                    if (!digit || count == 2) throw std::runtime_error("Formato invalido: se esperaban enteros por linea.");
                    int parsed = static_cast<int>(value);
                    if (negative) parsed = -parsed;
                    if (count == 0) a = parsed; else b = parsed;
                    ++count;
                    token = digit = negative = false;
                    value = 0;
                }
                if (c == '\n' || c == EOF) {
                    if (count) return true;
                    if (c == EOF && !any) return false;
                    if (c == EOF) return false;
                    break;
                }
            } else {
                any = true;
                if (!token && (c == '-' || c == '+')) {
                    token = true;
                    negative = c == '-';
                } else if (c >= '0' && c <= '9') {
                    token = digit = true;
                    value = value * 10 + static_cast<unsigned>(c - '0');
                    if (value > INT_MAX) throw std::runtime_error("Entero fuera del rango admitido.");
                } else {
                    throw std::runtime_error("Formato invalido: caracter distinto de entero o espacio.");
                }
            }
        }
    }
}
}

Tree::Tree(int count) : n(count), edges(static_cast<std::size_t>(n - 1)),
    head(static_cast<std::size_t>(n) + 1), tail(static_cast<std::size_t>(n) + 1),
    adjacency(static_cast<std::size_t>(n - 1) * 2) {
    for (int i = 0; i <= n; ++i) head[i] = tail[i] = -1;
}

void Tree::addEdge(int index, int u, int v) {
    edges[index] = {u, v};
    for (int direction = 0; direction < 2; ++direction) {
        const int from = direction == 0 ? u : v;
        const int to = direction == 0 ? v : u;
        adjacency[used] = {to, -1};
        if (head[from] == -1) head[from] = used;
        else adjacency[tail[from]].next = used;
        tail[from] = used++;
    }
}

Tree readTree(const char* path) {
    File file(path);
    int n = 0, other = 0, count = 0;
    if (!readRecord(file.handle, n, other, count) || count != 1 || n < 1)
        throw std::runtime_error("Primera linea invalida: se requiere n >= 1.");
    if (n > INT_MAX / 2) throw std::runtime_error("Demasiados nodos para los indices internos.");
    Tree tree(n);
    for (int i = 0; i < n - 1; ++i) {
        int u = 0, v = 0;
        if (!readRecord(file.handle, u, v, count) || count != 2)
            throw std::runtime_error("Entrada incompleta o arista mal formada: se requieren n-1 pares.");
        if (u < 1 || u > n || v < 1 || v > n)
            throw std::runtime_error("Extremo de arista fuera de [1,n].");
        if (u == v) throw std::runtime_error("No se permiten lazos (u == v).");
        tree.addEdge(i, u, v);
    }
    if (readRecord(file.handle, n, other, count))
        throw std::runtime_error("Datos adicionales: se requieren exactamente n-1 aristas.");
    return tree;
}

void validateTree(const Tree& tree, int start) {
    if (start < 1 || start > tree.n) throw std::runtime_error("--start debe estar en [1,n].");
    Array<int> marked(static_cast<std::size_t>(tree.n) + 1);
    // Marcas por origen: detecta duplicados y sus inversos en O(n), sin hash ni ordenar.
    for (int u = 1; u <= tree.n; ++u) {
        for (int e = tree.head[u]; e != -1; e = tree.adjacency[e].next) {
            const int v = tree.adjacency[e].to;
            if (marked[v] == u) throw std::runtime_error("Arista duplicada, incluida su orientacion inversa.");
            marked[v] = u;
        }
    }
    for (int i = 1; i <= tree.n; ++i) marked[i] = 0;
    Queue queue(static_cast<std::size_t>(tree.n));
    queue.enqueue(start);
    marked[start] = 1;
    int visited = 0;
    while (!queue.empty()) {
        const int u = queue.dequeue();
        ++visited;
        for (int e = tree.head[u]; e != -1; e = tree.adjacency[e].next) {
            const int v = tree.adjacency[e].to;
            if (!marked[v]) { marked[v] = 1; queue.enqueue(v); }
        }
    }
    if (visited != tree.n) throw std::runtime_error("Grafo no conexo: la entrada no es un arbol.");
}
