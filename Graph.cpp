#include "Graph.h"
#include "bedrock.h"

#include <atomic>
#include <algorithm>
#include <future>
#include <iostream>
#include <queue>

static unsigned int THREAD_COUNT = []() {
    if (auto env = std::getenv("TP_SIZE")) {
        auto result = std::atoi(env);
        if (result > 0) {
            std::cerr << "Using custom tp size: " << result << '\n';
            return static_cast<unsigned int>(result);
        }
    }
    std::cerr << "Using default hardware concurency: " << std::thread::hardware_concurrency() << '\n';
    return std::thread::hardware_concurrency();
}();

static auto pool = []() { return br::ThreadPool(THREAD_COUNT); }();

Graph::Graph(int vertices) : vertexCount_(vertices), matrix_(vertices) {}

void Graph::addEdge(int src, int dest)
{
    if (src < 0 || dest < 0 || src >= vertexCount_ || dest >= vertexCount_)
        return;
    auto &&vec = matrix_[src];
    if (std::find(vec.begin(), vec.end(), dest) == vec.end()) {
        vec.push_back(dest);
    }
}

struct alignas(64) AlignedBool {
    std::atomic<bool> flag;
};

void Graph::parallelBFS(int startVertex) const
{
    if (startVertex < 0 || startVertex >= vertexCount_)
        return;

    std::vector<AlignedBool> visited(vertexCount_);
    for (int i = 0; i < vertexCount_; ++i) {
        visited[i].flag.store(false, std::memory_order_relaxed);
    }

    std::vector<int> currentLevel;
    currentLevel.push_back(startVertex);
    visited[startVertex].flag.store(true, std::memory_order_relaxed);

    while (!currentLevel.empty()) {
        br::Mutex<std::vector<int>> nextLevel;
        size_t chunkSize = (currentLevel.size() + THREAD_COUNT - 1) / THREAD_COUNT;
        br::WaitGroup wg(1 + ((currentLevel.size() - 1) / chunkSize));
        for (size_t chunkStart = 0; chunkStart < currentLevel.size(); chunkStart += chunkSize) {
            size_t chunkEnd = std::min(chunkStart + chunkSize, currentLevel.size());
            pool.Push([&, chunkStart, chunkEnd] mutable {
                std::vector<int> localNextLevel;
                for (size_t i = chunkStart; i < chunkEnd; ++i) {
                    int u = currentLevel[i];
                    for (int v : matrix_[u]) {
                        bool expected = false;
                        if (visited[v].flag.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
                            localNextLevel.push_back(v);
                        }
                    }
                }
                if (!localNextLevel.empty()) {
                    auto nL = nextLevel.Lock();
                    nL->insert(nL->end(), localNextLevel.begin(), localNextLevel.end());
                }
                wg.Done();
            });
        }
        wg.Wait();
        currentLevel = std::move(*nextLevel.Lock());
    }
}

void Graph::bfs(int startVertex) const
{
    if (startVertex < 0 || startVertex >= vertexCount_)
        return;
    std::vector<char> visited(vertexCount_, 0);
    std::queue<int> q;

    visited[startVertex] = 1;
    q.push(startVertex);

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (int n : matrix_[u]) {
            if (!visited[n]) {
                visited[n] = 1;
                q.push(n);
            }
        }
    }
}

int Graph::vertices() const
{
    return vertexCount_;
}