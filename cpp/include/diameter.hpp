#pragma once
#include "bfs.hpp"

struct DiameterResult {
    int length, a, b;
    Array<int> pathNodes;
    explicit DiameterResult(const BFSResult& first, const BFSResult& second);
};

DiameterResult calculateDiameter(const Tree& tree, const BFSResult& first,
                                 const BFSResult& second, Trace& trace);
