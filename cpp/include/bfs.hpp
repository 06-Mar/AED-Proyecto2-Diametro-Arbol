#pragma once
#include "tree.hpp"

enum class EventType {
    PhaseStart, Discovered, Enqueued, Dequeued, Examined, Finished, PhaseEnd,
    DiameterPath, DiameterResult
};

struct Event {
    EventType type = EventType::PhaseStart;
    int phase = 0;
    int node = 0, other = 0, distance = 0, parent = -1;
    bool isNew = false;
};

class Trace {
    Array<Event> events_;
    std::size_t count_ = 0;
public:
    explicit Trace(int n) : events_(static_cast<std::size_t>(n) * 12 + 2) {}
    void add(EventType type, int phase, int node = 0, int other = 0,
             int distance = 0, int parent = -1, bool isNew = false);
    std::size_t size() const { return count_; }
    const Event& operator[](std::size_t index) const { return events_[index]; }
};

struct BFSResult {
    int start, farthestNode, maxDistance = 0;
    Array<int> distance, parent;
    BFSResult(int n, int source);
};

BFSResult bfsWithTrace(const Tree& tree, int start, int phase, Trace& trace);
