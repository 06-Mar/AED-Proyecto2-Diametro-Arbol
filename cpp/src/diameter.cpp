#include "diameter.hpp"

DiameterResult::DiameterResult(const BFSResult& first, const BFSResult& second)
    : length(second.maxDistance), a(first.farthestNode), b(second.farthestNode),
      pathNodes(static_cast<std::size_t>(length) + 1) {}

DiameterResult calculateDiameter(const Tree& tree, const BFSResult& first,
                                 const BFSResult& second, Trace& trace) {
    if (second.start != first.farthestNode) throw std::runtime_error("Origen de BFS 2 inconsistente.");
    DiameterResult result(first, second);
    int node = result.b;
    for (int i = result.length; i >= 0; --i) {
        if (node < 1 || node > tree.n || second.distance[node] != i)
            throw std::runtime_error("Cadena de padres inconsistente.");
        result.pathNodes[i] = node;
        node = second.parent[node];
    }
    if (result.pathNodes[0] != result.a || node != -1)
        throw std::runtime_error("Extremos del diametro inconsistentes.");
    // Cada enlace debe existir; cada nodo del camino se inspecciona una sola vez.
    for (int i = 0; i < result.length; ++i) {
        const int u = result.pathNodes[i], v = result.pathNodes[i + 1];
        bool found = false;
        for (int e = tree.head[u]; e != -1; e = tree.adjacency[e].next)
            if (tree.adjacency[e].to == v) { found = true; break; }
        if (!found) throw std::runtime_error("Arista inexistente en el camino.");
    }
    trace.add(EventType::DiameterPath, 3);
    trace.add(EventType::DiameterResult, 3);
    if (trace.size() != static_cast<std::size_t>(tree.n) * 12 + 2)
        throw std::runtime_error("Numero de eventos inconsistente.");
    return result;
}
