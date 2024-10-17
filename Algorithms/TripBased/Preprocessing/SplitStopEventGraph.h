#pragma once

#include <cassert>
#include <iostream> // For std::cout

namespace TripBased {

class SplitStopEventGraph {
public:
    SplitStopEventGraph(const Data& data)
        : data(data)
        , k(data.raptorData.numberOfPartitions)
        , numVertices(data.numberOfStopEvents())
        , numLocalEdges(0)
        , numTransferEdges(0)
    {
        incorporateGraph();
    };

    inline size_t beginLocalEdgeFrom(const size_t vertex) const
    {
        assert(isVertex(vertex) || vertex == numVertices);
        return toAdjLocal[vertex];
    }

    inline size_t beginTransferEdgeFrom(const size_t vertex) const
    {
        assert(isVertex(vertex) || vertex == numVertices);
        return toAdjTransfer[vertex];
    }

    inline size_t numberOfLocalEdges(const size_t vertex) const
    {
        assert(isVertex(vertex));
        return beginLocalEdgeFrom(vertex + 1) - beginLocalEdgeFrom(vertex);
    }

    inline size_t numberOfTransferEdges(const size_t vertex) const
    {
        assert(isVertex(vertex));
        return beginTransferEdgeFrom(vertex + 1) - beginTransferEdgeFrom(vertex);
    }

    inline bool isVertex(const size_t vertex) const
    {
        return (vertex < numVertices);
    }

    // Method to show detailed information
    void showInfo() const
    {
        std::cout << "SplitStopEventGraph Info:\n";
        std::cout << "   Number of vertices: " << numVertices << "\n";
        std::cout << "   Number of local edges: " << numLocalEdges << "\n";
        std::cout << "   Number of transfer edges: " << numTransferEdges << "\n";
        std::cout << "   Total number of edges: " << (numLocalEdges + numTransferEdges) << "\n";
        std::cout << "   Number of partitions (k): " << k << "\n";
    }

private:
    void incorporateGraph()
    {
        toAdjLocal.assign(numVertices + 1, 0);
        toAdjTransfer.assign(numVertices + 1, 0);

        toLocalVertex.reserve(data.stopEventGraph.numEdges());
        toTransferVertex.reserve(data.stopEventGraph.numEdges());

        size_t runningSumLocal = 0;
        size_t runningSumTransfer = 0;

        for (Vertex from(0); from < numVertices; ++from) {
            toAdjLocal[from] = runningSumLocal;
            toAdjTransfer[from] = runningSumTransfer;

            const StopId fromStop = data.getStopOfStopEvent(StopEventId(from));

            for (auto edge : data.stopEventGraph.edgesFrom(from)) {
                auto toVertex = data.stopEventGraph.get(ToVertex, edge);

                bool sameStop = (fromStop == data.getStopOfStopEvent(StopEventId(toVertex)));

                runningSumLocal += sameStop;
                runningSumTransfer += !sameStop;

                auto& toAddVec = (sameStop ? toLocalVertex : toTransferVertex);
                toAddVec.emplace_back(toVertex);
            }
        }

        toAdjLocal[numVertices] = runningSumLocal;
        toAdjTransfer[numVertices] = runningSumTransfer;

        numLocalEdges = runningSumLocal;
        numTransferEdges = runningSumTransfer;

        localFlags.resize(numLocalEdges, std::vector<bool>(k, false));
        transferFlags.resize(numTransferEdges, std::vector<bool>(k, false));
    }

    const Data& data;
    const int k;

    std::vector<size_t> toAdjLocal;
    std::vector<size_t> toAdjTransfer;

    std::vector<size_t> toLocalVertex;
    std::vector<size_t> toTransferVertex;

    std::vector<std::vector<bool>> localFlags;
    std::vector<std::vector<bool>> transferFlags;

    size_t numVertices;
    size_t numLocalEdges;
    size_t numTransferEdges;
};

}
