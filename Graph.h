#pragma once
#include <vector>

class Graph {
public:
    explicit Graph(int vertices);
    void addEdge(int src, int dest);
    void parallelBFS(int startVertex) const; // заглушка, как в Java
    void bfs(int startVertex) const;         // обычный BFS
    [[nodiscard]] int vertices() const;

private:
    int vertexCount_;
    std::vector<std::vector<int>> matrix_;
};