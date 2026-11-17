#pragma once
#include "structures.hpp"

struct Edge { int u = 0, v = 0; };
struct Adjacency { int to = 0, next = -1; };

// Listas enlazadas en arreglos: anexar por la cola conserva el orden de entrada.
struct Tree {
    int n;
    Array<Edge> edges;
    Array<int> head, tail;
    Array<Adjacency> adjacency;
    int used = 0;
    explicit Tree(int count);
    void addEdge(int index, int u, int v);
};

Tree readTree(const char* path);
void validateTree(const Tree& tree, int start);
