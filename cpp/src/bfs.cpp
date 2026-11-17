#include "bfs.hpp"

void Trace::add(EventType type, int phase, int node, int other, int distance, int parent, bool isNew) {
    if (count_ == events_.size()) throw std::runtime_error("Capacidad de traza excedida.");
    events_[count_++] = {type, phase, node, other, distance, parent, isNew};
}

BFSResult::BFSResult(int n, int source) : start(source), farthestNode(source),
    distance(static_cast<std::size_t>(n) + 1), parent(static_cast<std::size_t>(n) + 1) {
    for (int i = 0; i <= n; ++i) distance[i] = parent[i] = -1;
}

BFSResult bfsWithTrace(const Tree& tree, int start, int phase, Trace& trace) {
    BFSResult result(tree.n, start);
    Queue queue(static_cast<std::size_t>(tree.n));
    trace.add(EventType::PhaseStart, phase, start);
    result.distance[start] = 0;
    trace.add(EventType::Discovered, phase, start, 0, 0, -1);
    queue.enqueue(start);
    trace.add(EventType::Enqueued, phase, start);
    while (!queue.empty()) {
        const int u = queue.dequeue();
        trace.add(EventType::Dequeued, phase, u);
        for (int e = tree.head[u]; e != -1; e = tree.adjacency[e].next) {
            const int v = tree.adjacency[e].to;
            const bool isNew = result.distance[v] == -1;
            trace.add(EventType::Examined, phase, u, v, 0, -1, isNew);
            if (isNew) {
                result.distance[v] = result.distance[u] + 1;
                result.parent[v] = u;
                trace.add(EventType::Discovered, phase, v, 0, result.distance[v], u);
                queue.enqueue(v);
                trace.add(EventType::Enqueued, phase, v);
                if (result.distance[v] > result.maxDistance ||
                    (result.distance[v] == result.maxDistance && v < result.farthestNode)) {
                    result.maxDistance = result.distance[v];
                    result.farthestNode = v;
                }
            }
        }
        trace.add(EventType::Finished, phase, u);
    }
    trace.add(EventType::PhaseEnd, phase, start, result.farthestNode, result.maxDistance);
    return result;
}
