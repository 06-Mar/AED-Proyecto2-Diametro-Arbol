#include "json_export.hpp"
#include <cstdio>
#include <cstring>
#include <cerrno>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
std::FILE* openExclusive(const char* path) {
#ifdef _WIN32
    // El CRT de algunas distribuciones MinGW no admite el modo fopen "x".
    const int descriptor = _open(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                                 _S_IREAD | _S_IWRITE);
    if (descriptor == -1) return nullptr;
    std::FILE* file = _fdopen(descriptor, "wb");
    if (!file) {
        const int savedError = errno;
        _close(descriptor);
        std::remove(path);
        errno = savedError;
    }
    return file;
#else
    return std::fopen(path, "wbx");
#endif
}

void edgeId(std::FILE* f, int u, int v) {
    std::fprintf(f, "\"%d-%d\"", u < v ? u : v, u < v ? v : u);
}
void nodes(std::FILE* f, const DiameterResult& d) {
    std::fputc('[', f);
    for (int i = 0; i <= d.length; ++i) std::fprintf(f, "%s%d", i ? ", " : "", d.pathNodes[i]);
    std::fputc(']', f);
}
void edges(std::FILE* f, const DiameterResult& d) {
    std::fputc('[', f);
    for (int i = 0; i < d.length; ++i) {
        if (i) std::fputs(", ", f);
        edgeId(f, d.pathNodes[i], d.pathNodes[i + 1]);
    }
    std::fputc(']', f);
}
void parent(std::FILE* f, int value) {
    if (value == -1) std::fputs("null", f); else std::fprintf(f, "%d", value);
}
void run(std::FILE* f, const BFSResult& r, int phase, int n) {
    std::fprintf(f, "    {\"id\": \"bfs_%d\", \"start\": %d, \"farthest_node\": %d, \"max_distance\": %d,\n     \"nodes\": [\n",
                 phase, r.start, r.farthestNode, r.maxDistance);
    for (int i = 1; i <= n; ++i) {
        std::fprintf(f, "      {\"node\": %d, \"distance\": %d, \"parent\": ", i, r.distance[i]);
        parent(f, r.parent[i]);
        std::fprintf(f, "}%s\n", i < n ? "," : "");
    }
    std::fputs("     ]}", f);
}
void event(std::FILE* f, const Event& e, std::size_t step, const DiameterResult& d) {
    const char* phase = e.phase == 1 ? "bfs_1" : e.phase == 2 ? "bfs_2" : "result";
    std::fprintf(f, "    {\"step\": %zu, \"phase\": \"%s\", \"type\": ", step, phase);
    switch (e.type) {
    case EventType::PhaseStart:
        std::fprintf(f, "\"phase_start\", \"start\": %d", e.node); break;
    case EventType::Discovered:
        std::fprintf(f, "\"node_discovered\", \"node\": %d, \"distance\": %d, \"parent\": ", e.node, e.distance);
        parent(f, e.parent); break;
    case EventType::Enqueued:
        std::fprintf(f, "\"node_enqueued\", \"node\": %d", e.node); break;
    case EventType::Dequeued:
        std::fprintf(f, "\"node_dequeued\", \"node\": %d", e.node); break;
    case EventType::Examined:
        std::fprintf(f, "\"edge_examined\", \"from\": %d, \"to\": %d, \"edge_id\": ", e.node, e.other);
        edgeId(f, e.node, e.other);
        std::fprintf(f, ", \"outcome\": \"%s\"", e.isNew ? "new" : "already_seen"); break;
    case EventType::Finished:
        std::fprintf(f, "\"node_finished\", \"node\": %d", e.node); break;
    case EventType::PhaseEnd:
        std::fprintf(f, "\"phase_end\", \"start\": %d, \"farthest_node\": %d, \"max_distance\": %d", e.node, e.other, e.distance); break;
    case EventType::DiameterPath:
        std::fputs("\"diameter_path\", \"nodes\": ", f); nodes(f, d);
        std::fputs(", \"edges\": ", f); edges(f, d); break;
    case EventType::DiameterResult:
        std::fprintf(f, "\"diameter_result\", \"endpoints\": [%d, %d], \"length\": %d", d.a, d.b, d.length); break;
    }
    std::fputc('}', f);
}

// El temporal se crea exclusivamente junto al destino. El destino anterior
// solo se sustituye despues de comprobar fflush y fclose.
class Output {
    Array<char> temporary_;
    bool committed_ = false;
public:
    std::FILE* file = nullptr;
    explicit Output(const char* path) : temporary_(std::strlen(path) + 40) {
        for (unsigned attempt = 0; attempt < 1000; ++attempt) {
            std::snprintf(temporary_.data(), temporary_.size(), "%s.tmp.%u", path, attempt);
            file = openExclusive(temporary_.data());
            if (file) return;
            if (errno != EEXIST) break;
        }
        throw std::runtime_error("No se pudo crear el temporal de salida; compruebe carpeta y permisos.");
    }
    ~Output() {
        if (file) std::fclose(file);
        if (!committed_) std::remove(temporary_.data());
    }
    void commit(const char* path) {
        bool failed = std::ferror(file) != 0;
        if (std::fflush(file) != 0) failed = true;
        if (std::fclose(file) != 0) failed = true;
        file = nullptr;
        if (failed) throw std::runtime_error("Fallo al escribir o cerrar el JSON; destino conservado.");
#ifdef _WIN32
        if (!MoveFileExA(temporary_.data(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
#else
        if (std::rename(temporary_.data(), path) != 0)
#endif
            throw std::runtime_error("No se pudo renombrar el JSON temporal; destino conservado.");
        committed_ = true;
    }
};
}

void exportJson(const char* outputPath, const Tree& tree, const BFSResult& first,
                const BFSResult& second, const DiameterResult& d, const Trace& trace) {
    if (trace.size() != static_cast<std::size_t>(tree.n) * 12 + 2)
        throw std::runtime_error("Cantidad de eventos inconsistente.");
    Output output(outputPath);
    std::FILE* f = output.file;
    std::fprintf(f, "{\n  \"schema_version\": \"1.0\",\n  \"algorithm\": \"tree_diameter_two_bfs\",\n  \"graph\": {\n    \"type\": \"undirected_unweighted_tree\",\n    \"node_count\": %d,\n    \"nodes\": [", tree.n);
    for (int i = 1; i <= tree.n; ++i) std::fprintf(f, "%s%d", i > 1 ? ", " : "", i);
    std::fputs("],\n    \"edges\": [", f);
    for (int i = 0; i < tree.n - 1; ++i) {
        const Edge& e = tree.edges[i];
        std::fputs(i ? ",\n      {\"id\": " : "\n      {\"id\": ", f);
        edgeId(f, e.u, e.v);
        std::fprintf(f, ", \"source\": %d, \"target\": %d}", e.u, e.v);
    }
    std::fprintf(f, "]\n  },\n  \"config\": {\"initial_node\": %d, \"neighbor_order\": \"input_order\", \"farthest_tie_breaker\": \"smallest_node_id\", \"distance_unit\": \"edges\"},\n  \"runs\": [\n", first.start);
    run(f, first, 1, tree.n); std::fputs(",\n", f); run(f, second, 2, tree.n);
    std::fprintf(f, "\n  ],\n  \"diameter\": {\"length\": %d, \"endpoints\": [%d, %d], \"path_nodes\": ", d.length, d.a, d.b);
    nodes(f, d); std::fputs(", \"path_edges\": ", f); edges(f, d);
    std::fputs("},\n  \"events\": [\n", f);
    for (std::size_t i = 0; i < trace.size(); ++i) {
        event(f, trace[i], i, d);
        std::fputs(i + 1 < trace.size() ? ",\n" : "\n", f);
    }
    std::fprintf(f, "  ],\n  \"stats\": {\"bfs_runs\": 2, \"events_count\": %zu}\n}\n", trace.size());
    output.commit(outputPath);
}
