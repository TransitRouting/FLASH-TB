#pragma once

#include "Profiler.h"
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "../../DataStructures/Attributes/AttributeNames.h"
#include "../../DataStructures/Container/ExternalKHeap.h"
#include "../../DataStructures/Container/Set.h"
#include "../../DataStructures/TD/Data.h"
#include "../../Helpers/Meta.h"
#include "../../Helpers/String/String.h"
#include "../../Helpers/Timer.h"
#include "../../Helpers/Types.h"
#include "../../Helpers/Vector/Vector.h"

namespace TD {

template <typename GRAPH, typename PROFILER = NoProfiler, bool DEBUG = false>
class EADijkstra {
public:
    using Graph = GRAPH;
    using Profiler = PROFILER;
    static constexpr bool Debug = DEBUG;
    using Type = EADijkstra<Graph, Profiler, Debug>;

public:
    struct VertexLabel : public ExternalKHeapElement {
        VertexLabel() : ExternalKHeapElement(), arrivalTime(intMax), parent(noVertex), timeStamp(-1) {}
        inline void reset(int time) {
            arrivalTime = intMax;
            parent = noVertex;
            timeStamp = time;
        }
        inline bool hasSmallerKey(const VertexLabel* other) const { return arrivalTime < other->arrivalTime; }

        int arrivalTime;
        Vertex parent;
        int timeStamp;
    };

public:
    EADijkstra(const GRAPH& graph, const std::vector<std::vector<std::pair<uint32_t, uint32_t>>>& times)
        : graph(graph), times(times), Q(graph.numVertices()), label(graph.numVertices()), timeStamp(0), settleCount(0) {
        profiler.registerPhases({PHASE_CLEAR, PHASE_RUN});
        profiler.registerMetrics({METRIC_SEETLED_VERTICES, METRIC_RELAXED_TRANSFER_EDGES, METRIC_RELAXED_ROUTE_EDGES,
                                  METRIC_FOUND_SOLUTIONS});
    }

    template <AttributeNameType ATTRIBUTE_NAME>
    EADijkstra(const GRAPH& graph, const AttributeNameWrapper<ATTRIBUTE_NAME>& weight)
        : EADijkstra(graph, graph[weight]) {}

    EADijkstra(const GRAPH&&, const std::vector<int>&) = delete;
    EADijkstra(const GRAPH&, const std::vector<int>&&) = delete;
    EADijkstra(const GRAPH&&) = delete;

    template <AttributeNameType ATTRIBUTE_NAME>
    EADijkstra(const GRAPH&&, const AttributeNameWrapper<ATTRIBUTE_NAME>&) = delete;

    template <typename SETTLE = NO_OPERATION, typename STOP = NO_OPERATION, typename PRUNE_EDGE = NO_OPERATION>
    inline void run(const Vertex source, const int departureTime = 0, const Vertex target = noVertex,
                    const SETTLE& settle = NoOperation, const STOP& stop = NoOperation,
                    const PRUNE_EDGE& pruneEdge = NoOperation) noexcept {
        profiler.start();

        clear();
        addSource(source, departureTime);
        run(target, settle, stop, pruneEdge);

        profiler.done();

        if (getDistance(target) != intMax) profiler.countMetric(METRIC_FOUND_SOLUTIONS);
    }

    template <typename SETTLE = NO_OPERATION, typename STOP = NO_OPERATION, typename PRUNE_EDGE = NO_OPERATION>
    inline void run(const Vertex source, IndexedSet<false, Vertex>& targets, const SETTLE& settle = NoOperation,
                    const STOP& stop = NoOperation, const PRUNE_EDGE& pruneEdge = NoOperation) noexcept {
        clear();
        addSource(source);
        run(
            noVertex,
            [&](const Vertex u) {
            settle(u);
            targets.remove(u);
            },
            [&]() { return stop() || targets.empty(); }, pruneEdge);
    }

    template <typename SOURCE_CONTAINER, typename SETTLE = NO_OPERATION, typename STOP = NO_OPERATION,
              typename PRUNE_EDGE = NO_OPERATION, typename = decltype(std::declval<SOURCE_CONTAINER>().begin())>
    inline void run(const SOURCE_CONTAINER& sources, const Vertex target = noVertex, const SETTLE& settle = NoOperation,
                    const STOP& stop = NoOperation, const PRUNE_EDGE& pruneEdge = NoOperation) noexcept {
        clear();
        for (const Vertex source : sources) {
            addSource(source);
        }
        run(target, settle, stop, pruneEdge);
    }

    inline void clear() noexcept {
        profiler.startPhase();

        if constexpr (Debug) {
            timer.restart();
            settleCount = 0;
        }
        Q.clear();
        timeStamp++;

        profiler.donePhase(PHASE_CLEAR);
    }

    inline void addSource(const Vertex source, const int arrivalTime = 0) noexcept {
        VertexLabel& sourceLabel = getLabel(source);
        sourceLabel.arrivalTime = arrivalTime;
        Q.update(&sourceLabel);
    }

    inline void run() noexcept { run(noVertex, NoOperation, NoOperation, NoOperation); }

    template <typename SETTLE, typename STOP = NO_OPERATION, typename PRUNE_EDGE = NO_OPERATION,
              typename = decltype(std::declval<SETTLE>()(std::declval<Vertex>()))>
    inline void run(const Vertex target, const SETTLE& settle, const STOP& stop = NoOperation,
                    const PRUNE_EDGE& pruneEdge = NoOperation) noexcept {
        profiler.startPhase();

        while (!Q.empty()) {
            if (stop()) break;
            VertexLabel* uLabel = Q.extractFront();
            const Vertex u = Vertex(uLabel - &(label[0]));
            if (u == target) [[unlikely]]
                break;
            for (const Edge edge : graph.edgesFrom(u)) {
                const Vertex v = graph.get(ToVertex, edge);
                VertexLabel& vLabel = getLabel(v);
                if (pruneEdge(u, edge)) continue;
                // duration != -1 => footpath
                int arrivalTime = uLabel->arrivalTime;
                int durationToAdd = 0;

                if (graph.get(TravelTime, edge) != -1) {
                    profiler.countMetric(METRIC_RELAXED_TRANSFER_EDGES);
                    durationToAdd = graph.get(TravelTime, edge);
                } else {
                    profiler.countMetric(METRIC_RELAXED_ROUTE_EDGES);
                    assert(!graph.get(DurationFunction, edge).empty());
                    durationToAdd = evaluateEdgeBinary(graph.get(DurationFunction, edge), arrivalTime);
                }
                if (durationToAdd == intMax) [[unlikely]] {
                    continue;
                }
                arrivalTime += durationToAdd;
                if (vLabel.arrivalTime > arrivalTime) {
                    vLabel.arrivalTime = arrivalTime;
                    vLabel.parent = u;
                    Q.update(&vLabel);
                }
            }
            profiler.countMetric(METRIC_SEETLED_VERTICES);

            settle(u);
            if constexpr (Debug) settleCount++;
        }
        if constexpr (Debug) {
            std::cout << "Settled Vertices = " << String::prettyInt(settleCount) << std::endl;
            std::cout << "Time = " << String::msToString(timer.elapsedMilliseconds()) << std::endl;
        }

        profiler.donePhase(PHASE_RUN);
    }

    inline bool reachable(const Vertex vertex) const noexcept { return label[vertex].timeStamp == timeStamp; }

    inline bool visited(const Vertex vertex) const noexcept { return label[vertex].timeStamp == timeStamp; }

    inline int getDistance(const Vertex vertex) const noexcept {
        return (visited(vertex) ? label[vertex].arrivalTime : -1);
    }

    inline Vertex getParent(const Vertex vertex) const noexcept {
        return (visited(vertex) ? label[vertex].parent : noVertex);
    }

    inline std::set<Vertex> getChildren(const Vertex vertex) const noexcept {
        if (visited(vertex)) {
            std::set<Vertex> children;
            for (Vertex child : graph.outgoingNeighbors(vertex)) {
                if (label[child].parent == vertex) {
                    children.insert(child);
                }
            }
            return children;
        }
        return std::set<Vertex>();
    }

    inline Vertex getQFront() const noexcept { return (Q.empty() ? noVertex : Vertex(Q.front() - &(label[0]))); }

    inline std::vector<Vertex> getReversePath(const Vertex to) const noexcept {
        std::vector<Vertex> path;
        if (!visited(to)) return path;
        path.push_back(to);
        while (label[path.back()].parent != noVertex) {
            path.push_back(label[path.back()].parent);
        }
        return path;
    }

    inline std::vector<Vertex> getPath(const Vertex to) const noexcept { return Vector::reverse(getReversePath(to)); }

    inline int getSettleCount() const noexcept { return settleCount; }

    inline const Profiler& getProfiler() const noexcept { return profiler; }

private:
    inline VertexLabel& getLabel(const Vertex vertex) noexcept {
        VertexLabel& result = label[vertex];
        if (result.timeStamp != timeStamp) [[unlikely]]
            result.reset(timeStamp);
        return result;
    }

    inline int evaluateEdgeLinear(const std::vector<std::pair<uint32_t, uint32_t>>& times,
                                  const int arrivalTime) const {
        AssertMsg(!times.empty(), "Given DurationFunction is empty?");
        AssertMsg(times.back().first == intMax, "The end should contain a sentinel!");
        AssertMsg(times.back().second == intMax, "The end should contain a sentinel!");
        AssertMsg(arrivalTime < intMax, "ArrivalTime is infinity?");
        size_t i = 0;

        while (i < times.size() && times[i].first < static_cast<uint32_t>(arrivalTime))
            ++i;

        assert(i < times.size());
        return times[i].second;
    }

    inline int evaluateEdgeBinary(const std::vector<std::pair<uint32_t, uint32_t>>& times,
                                  const int arrivalTime) const {
        AssertMsg(!times.empty(), "Given DurationFunction is empty?");
        AssertMsg(times.back().first == intMax, "The end should contain a sentinel!");
        AssertMsg(times.back().second == intMax, "The end should contain a sentinel!");
        AssertMsg(arrivalTime < intMax, "ArrivalTime is infinity?");

        // Use std::lower_bound to find the first element that is not less than arrivalTime
        auto it = std::lower_bound(times.begin(), times.end(), arrivalTime, [](const auto& pair, const int value) {
            return pair.first < static_cast<uint32_t>(value);
        });

        // We assume that the last element is the sentinel, so it should always find a valid iterator.
        assert(it != times.end());
        return it->second;
    }

private:
    const GRAPH& graph;
    const std::vector<std::vector<std::pair<uint32_t, uint32_t>>>& times;

    ExternalKHeap<2, VertexLabel> Q;

    std::vector<VertexLabel> label;
    int timeStamp;

    int settleCount;
    Timer timer;

    Profiler profiler;
};
} // namespace TD
