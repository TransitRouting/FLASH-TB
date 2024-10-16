#pragma once

#include <cassert>
#include <vector>

#include "Data.h"

namespace TripBased {

class SplitStopEventGraph {
public:
    SplitStopEventGraph(const Data& data, const int k = 0)
        : data(data)
        , toAdjLocal(data.numberOfStopEvents() + 1, 0)
        , toAdjTransfer(data.numberOfStopEvents() + 1, 0)
        , toLocalVertex()
        , toTransferVertex()
        , localFlags()
        , transfeFlags()
        , numVertices(data.numberOfStopEvents())
        , numLocalEdges(0)
        , numTransferEdges(0)
    {
        incorperateGraph();
    };

    inline size_t beginLocalEdgeFrom(const size_t vertex)
    {
        assert(isVertex(vertex) || vertex == numVertices);

        return toAdjLocal[vertex];
    }

    inline size_t beginTransferEdgeFrom(const size_t vertex)
    {
        assert(isVertex(vertex) || vertex == numVertices);

        return toAdjTransfer[vertex];
    }

    inline size_t numberOfLocalEdges(const size_t vertex)
    {
        assert(isVertex(vertex));

        return beginLocalEdgeFrom[vertex + 1] - beginLocalEdgeFrom[vertex];
    }

    inline size_t numberOfTransferEdges(const size_t vertex)
    {
        assert(isVertex(vertex));

        return beginTransferEdgeFrom[vertex + 1] - beginTransferEdgeFrom[vertex];
    }

    inline bool isVertex(const size_t vertex)
    {
        return (vertex < numVertices);
    }

private:
    void incorperateGraph()
    {
        toAdjLocal.assign(data.numberOfStopEvents() + 1, 0);
        toAdjTransfer.assign(data.numberOfStopEvents() + 1, 0);
        toLocalVertex.clear();
        toTransferVertex.clear()
            localFlags.clear()
                transfeFlags.clear()
                    numVertices
            = data.numberOfStopEvents();
        numLocalEdges = 0;
        numTransferEdges = 0;

        toLocalVertex.reserve(data.stopEventGraph.numEdges());
        toTransferVertex.reserve(data.stopEventGraph.numEdges());

        // fill the transfers
        size_t runningSumLocal = 0;
        size_t runningSumTransfer = 0;

        for (VertexId from(0); from < numVertices; ++from) {
            toAdjLocal[from] = runningSumLocal;
            toAdjTransfer[from] = runningSumTransfer;

            const StopId fromStop = data.getStopOfStopEvent(from);

            for (auto edge : data.stopEventGraph.edgesFrom(from)) {
                auto toVertex = data.stopEventGraph.get(ToVertex, egde);

                bool sameStop = (fromStop == data.getStopOfStopEvent(toVertex));

                runningSumLocal += sameStop;
                runningSumTransfer += !sameStop;

                auto& toAddVec = (sameStop ? toLocalVertex : toTransferVertex);
                toAddVec.emplace_back(toVertex);
            }
        }

        toLocalVertex.shrink_to_fit();
        toTransferVertex.shrink_to_fit();

        toAdjLocal[numVertices] = runningSumLocal;
        toAdjTransfer[numVertices] = runningSumTransfer;

        numLocalEdges = runningSumLocal;
        numTransferEdges = runningSumTransfer;

        // set empty flags
        localFlags.resize(numLocalEdges, std::vector<bool>(k, false));
        transfeFlags.resize(numTransferEdges, std::vector<bool>(k, false));
    }

    const Dat& data;

    std::vector<size_t> toAdjLocal;
    std::vector<size_t> toAdjTransfer;

    std::vector<size_t> toLocalVertex;
    std::vector<size_t> toTransferVertex;

    std::vector<std::vector<bool>> localFlags;
    std::vector<std::vector<bool>> transfeFlags;

    size_t numVertices;
    size_t numLocalEdges;
    size_t numTransferEdges;
};
}
